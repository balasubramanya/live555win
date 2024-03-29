#include ".\uploadhandler.h"
#include <string>
#include <fstream>
#include <algorithm>
#include <map>
#include <windows.h>
using namespace std;
//
//#include "RaProtocolStruct.h"
//#include "RaProtoValue.h"
#include "debug.h"
#include "sysdata.h"
#include "httppost.h"
#include "nodeclient.h"
#include "mime.h"
#include "NodeClient.h"

//
//typedef FILE * (*FUN_fopen)(const char*, int, ...);
//typedef size_t (*FUN_fread)(int, void*, unsigned int);
//typedef size_t (*FUN_fwrite)(int, const void*, unsigned int);
//typedef int    (*FUN_fseek)(int, long, int);
//typedef  long  (*FUN_ftell)(int);
//typedef int    (*FUN_fclose)(int);
//typedef int    (*FUN_stat) (const char*, struct stat*);
//typedef  int   (*FUN_fstat) (int, struct stat*);

const string GetExePath(){
	char szFilePath[MAX_PATH + 1]={0};
	GetModuleFileName(NULL, szFilePath, MAX_PATH);
	(strrchr(szFilePath, ('\\')))[1] = 0;//删除文件名，只获得路径字串
	return szFilePath;
}

//const string strServDir = GetExePath() + "website";

UploadHandler::UploadHandler(HttpUploader *pUploader)
{	
	//mimeinit();
	//string strMime = ext2mime("htm");
	m_pUploader = pUploader;
}

UploadHandler::~UploadHandler(void)
{
}

void RespMsg(SP_HttpResponse * response,const string &msg,const string &head=""){
	std::string strResp = (string)"<html><head><meta http-equiv=\"content-type\" content=\"text/html; charset=gbk\" /> <title>并行编译大师</title>" 
		+ head 
		+ "</head><body>" 
		+ msg 
		+ "</body></html>";
	response->addHeader("Content-Type","text/html; charset=GBK");
	response->appendContent((char *)strResp.c_str(),strResp.size());
}
void RespJson(SP_HttpResponse * response,const string &msg){
	//response->addHeader("Content-Type","text/html; charset=GBK");
	response->appendContent((char *)msg.c_str(),msg.size());
}


FILE *fp;
void findFile(const char filePath[], string &strRes )
{
	char szFind[MAX_PATH];
	WIN32_FIND_DATA FindFileData;
	HANDLE hFind;
	char szFile[MAX_PATH];

	strRes = "<ul>";

	strcpy(szFind,filePath);
	strcat(szFind,"\\*.*");//利用通配符找这个目录下的所以文件，包括目录

	hFind=FindFirstFile(szFind,&FindFileData);
	if(INVALID_HANDLE_VALUE == hFind)    return;

	while(TRUE)
	{

		if(FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)//这是目录
		{
			if(string(FindFileData.cFileName)!="."){
				strRes += (string)"<li><a href='" + FindFileData.cFileName + "/'>[目录]" + FindFileData.cFileName + "</a></li>";
			}
			
		}
		else
		{
			strRes += (string)"<li><a href='" + FindFileData.cFileName + "'>" + FindFileData.cFileName + "</a></li>";
		}
		
		if(!FindNextFile(hFind,&FindFileData))//寻找下一个文件
			break;
	}
	strRes += "</ul>";
	FindClose(hFind);//关闭句柄
}
//请求的格式如下："/getFile?id=%d&fid=%s&pos=%d&size=%d";
void UploadHandler::handle( SP_HttpRequest * request, SP_HttpResponse * response )
{
	string url = request->getURI();
	cout<<"URL:"<<request->getURL()<<endl;

	T_mapHandler::iterator it = m_mapHandler.find(url);
	if (it!=m_mapHandler.end())
	{
		(*it).second(request,response);
		return;
	}

	if(std::string("/")==url){
		url = "/index.htm"; //url重定向
	}
	//string full_path = strServDir + url;
	string full_path = m_strPath + url;
	string ext = url.substr(url.rfind(".")+1);
	transform(ext.begin(),ext.end(),ext.begin(),tolower);
	string strMime = Mime::ext2mime(ext);
	if(!strMime.empty()){
		response->addHeader("Content-Type",strMime.c_str());
	}
	const char *fid = request->getParamValue("fid");
	const char *pos = request->getParamValue("pos");
	const char *size = request->getParamValue("size");
	int iPos = 0;
	unsigned int iSize = -1;
	if(pos){
		iPos = atoi(pos);
		iSize = atoi(size);
		if (iPos<0 || iSize<=0)
		{
			RespMsg(response,"文件位置或大小参数错误！");
			return;
		}
	}
	//////////////////////////////////////////////////////////////////////////
	//打开文件
	//////////////////////////////////////////////////////////////////////////
	std::ifstream is(full_path.c_str(), std::ios::in | std::ios::binary);
	if (!is)
	{
		RespMsg(response,"file not found!");
		return;
	}

	is.seekg(iPos);
	if(is.eof() || is.fail())  //fuck,seekg居然连个返回false都不做
	{
		RespMsg(response,"file error!");
		return;
	}		
	//发送数据体
	unsigned int iLeftSize = iSize;
	while (iLeftSize>0)
	{
		int iReadSize = is.read(m_pBuffer, min(iLeftSize,c_iBUFFER_SIZE)).gcount();
		iLeftSize -= iReadSize;
		if(iReadSize <= 0)
		{
			break;
		}
		response->appendContent(m_pBuffer, iReadSize);
	}
	return;
}

void UploadHandler::ReturnEmptyBlock( SP_HttpResponse * response, const MD5 &fid, int iPos)
{
	//构造数据包
	//int iTotalReplyPackLen = sizeof(RaPack) +sizeof(RA_GetData_Ret);
	//boost::shared_ptr<RaPack> pPackReply((RaPack *)malloc(iTotalReplyPackLen),free);
	//pPackReply->byVersion = c_byVersion;
	//pPackReply->dwCMDType = RA_C2C_GetData_Ret;
	//pPackReply->dwProtoFlag = c_byProtoType_C2C;
	//pPackReply->usLen = iTotalReplyPackLen;
	//pPackReply->dwReceiverId = 0;
	//pPackReply->dwSenderId = 0;

	//RA_GetData_Ret *pGetDataRet = (RA_GetData_Ret *)pPackReply->ucData;
	//pGetDataRet->mdChannelHash = fid;
	//pGetDataRet->dwPos = iPos;
	//pGetDataRet->dwDataLen = 0;

	//response->appendContent((char *)pPackReply.get(),iTotalReplyPackLen);
}

void UploadHandler::addHandler( const char *url,pHttpHandler handler )
{
	m_mapHandler.insert(T_mapHandler::value_type(url,handler));
}

void UploadHandler::setHomePath( const char *path)
{
	m_strPath = path;
}
string UploadHandler::m_strPath = GetExePath() + "website";
UploadHandler::T_mapHandler UploadHandler::m_mapHandler;
