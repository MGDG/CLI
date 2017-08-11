/**
  ******************************************************************************
  * @file    CMD_Process.c
  * @author  mgdg
  * @version V1.0.0
  * @date    2017-08-09
  * @brief   
  ******************************************************************************
 **/

#include "CMD_Process.h"

#define CMD_MAXPARANUM 		20				//支持的最大参数个数(包括命令在内)
#define CMD_STRLEN 		512				//命令字符串的最大长度(包括参数在内)

static void CMDHandle_hello(uint8_t paranum,const char *para[]);
static void CMDHandle_Help(uint8_t paranum,const char *para[]);
static void CMDHandle_cmdtest(uint8_t paranum,const char *para[]);

//结构体定义
static const struct
{
	const char *cmd_name;					//命令字符串
	const char *cmd_help;					//命令帮助信息
	void (*cmd_handle)(uint8_t paranum,const char *para[]);	//回调函数定义
}CMD_List[]=
{
	{"hello","printf hello",CMDHandle_hello},
	{"help","printf all command help information",CMDHandle_Help},
	{"cmdtest","cmd test,printf all parameter e.g: (cmdtest <para1> <para2> ...)",CMDHandle_cmdtest},

	{NULL,NULL,NULL}
};


//获取命令字符串中的参数个数以及每个参数的起始指针，参数之间由空格隔开
//参数个数包括命令本身
//并将每个参数后的第一个空格替换成空字符
static uint8_t CMD_GetPara(char *strbuf,char **paramn)
{
	uint16_t i = 0;
	volatile uint8_t paranum = 0;
	uint16_t para_len = 0;
	uint16_t strbuflen;
	
	if(strbuf==NULL || paramn==NULL)
		return paranum;
	
	strbuflen = strlen(strbuf);

	while(paranum < CMD_MAXPARANUM)
	{
		while(strbuf[i] == 0x20)			//Filter out spaces before parameter
		{
			i++;
			if(i>=strbuflen)
				return paranum;
		}

		paramn[paranum] = &strbuf[i];		//Get parameter pointers
		
		for(;( (strbuf[i]!=0x20) && (strbuf[i]!='\0') );i++)
		{
			if(i>=strbuflen)
				return paranum+para_len;

			para_len = 1;
		}
		strbuf[i++] = '\0';					//Replace with null characters

		if(para_len)
		{
			paranum++;
			para_len = 0;
		}
		else
			return paranum;
	}
	return paranum;
}

//解析命令字符串
static void CMD_Process(char *strbuf)
{
	char *param[CMD_MAXPARANUM];					//存放参数指针
	uint8_t paranum = CMD_GetPara(strbuf,(char **)param);

	if(paranum == 0)
		return;

	for(uint8_t i=0;CMD_List[i].cmd_name != NULL ;i++)
	{
		if(CMD_List[i].cmd_name[0] != param[0][0])		//skip if the first characters is different
			continue;
		if(strcmp(CMD_List[i].cmd_name,param[0]) == 0)
		{
			if(CMD_List[i].cmd_handle != NULL)
				CMD_List[i].cmd_handle(paranum,(const char **)param);
			return;
		}
	}

	DEBUGOUT("Can't find the command\r\n");
}


//弹掉队列中非有效字母或数字的队尾，弹完后返回剩余队列长度
static uint16_t CMD_FindStart(queue_Typedef *queue)
{
	uint8_t temp;
	uint16_t index=0;
	
	while(1)
	{
		if(!queue_Peek_Offset(queue,&temp,index))
			break;
		
		if((temp>='a' && temp <= 'z') || (temp>='A' && temp <= 'Z'))
			break;
		
		else
			index++;
	}
	
	if(index)
		queue_PopLength(queue,index);
	
	return queue_GetCount(queue);
}

//从队列中提取一条命令字符串
void CMD_UnPack(queue_Typedef *queue)
{
	bool end_flag = false;
	uint8_t temp;
	uint16_t pack_length;
	uint16_t cmd_length;
	char cmd_buf[CMD_STRLEN];							//存放接收到的整个命令包括参数
	
	if(cmd_buf == NULL)
		return;
	
	pack_length = CMD_FindStart(queue);				//弹掉无效的起始字符
	
	if(pack_length == 0)
		return;
	
	for(cmd_length=0;cmd_length<pack_length-1;cmd_length++)
	{
		queue_Peek_Offset(queue,&temp,cmd_length);
		if(temp == 0x0D)
		{
			queue_Peek_Offset(queue,&temp,cmd_length+1);
			if(temp == 0x0A)
			{
				end_flag = true;		//找到结尾
				break;
			}
		}
	}
	if(end_flag)
	{
		if(cmd_length >= CMD_STRLEN)
			cmd_length = CMD_STRLEN - 1;								//命令超过长度会被截取
		queue_PeekPopLength(queue,(uint8_t*)cmd_buf,cmd_length);		//取出指定长度并弹出
		cmd_buf[cmd_length] = '\0';										//末尾补上空字符串
		queue_PopLength(queue,2);										//弹掉回车换行符
		CMD_Process(cmd_buf);
	}
	else
	{
		queue_PopLength(queue,pack_length);								//没找到回车符，弹掉数据
	}
}

static void CMDHandle_hello(uint8_t paranum,const char *para[])
{
	DEBUGOUT("hello\r\n");
}

static void CMDHandle_Help(uint8_t paranum,const char *para[])
{
	uint8_t count,left;
	
	DEBUGOUT("\r\nCMD List:\r\n\r\n");
	for(uint8_t i=0;CMD_List[i].cmd_name != NULL ;i++)
	{
		//让打印的信息对齐
		count = (strlen(CMD_List[i].cmd_name)+1)/8;				//制表符的大小根据串口助手而定，可能为4，可能为8，此处使用的SecureCRT为8
		
		if(count <= 3)
			left = 3-count;
		else
			left = 0;
		
		DEBUGOUT("%s:",CMD_List[i].cmd_name);
		
		for(uint8_t j=0;j<left;j++)
			DEBUGOUT("\t");
		
		DEBUGOUT("%s\r\n",CMD_List[i].cmd_help);
	}
}

static void CMDHandle_cmdtest(uint8_t paranum,const char *para[])
{
	for(uint8_t i=0;i<paranum;i++)
	{
		DEBUGOUT("parameter %d : %s\r\n",i,para[i]);
	}
}

