#ifndef _PROXY_H
#define _PROXY_H

#include <WinSock2.h>
#include "windows.h"
#include <iostream>
#include <malloc.h>
#include <process.h>
#include <Winhttp.h>
#include "../transparent_wnd.h"
#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"winhttp.lib")
using namespace std;

#define MAXBUFFERSIZE      20*1024      //缓冲区大小
#define HTTP  "http://"
#define HTTPS  "CONNECT "

#define DEBUG 1

//宏定义
#define BLOCK 1024
#define MAXNUM 8
#define MAX_LINK_COUNT 100

class MelodyProxy{
public:
	//Proxy 端口
	CRITICAL_SECTION cs;		//临界段
	UINT pport;// = 8888;
	bool agentRequest;
	bool replaceResponse;
	TransparentWnd* winHandler;
	static int linkCount;
	static map<wstring, wstring> proxyMap;
	MelodyProxy(){
		pport=8888;
		agentRequest=false;
		replaceResponse=false;
	}
	MelodyProxy(TransparentWnd* _winHandler, UINT _pport=8888, bool _replaceResponse=false){
		pport=_pport;
		agentRequest=true;
		replaceResponse=_replaceResponse;
		winHandler=_winHandler;
	}

	struct ProxySockets
	{
		SOCKET  ProxyToUserSocket;	//本地机器到PROXY 服务机的socket
		SOCKET  ProxyToServSocket;	//PROXY 服务机到远程主机的socket
		BOOL    IsProxyToUserClosed; // 本地机器到PROXY 服务机状态
		BOOL    IsProxyToServClosed; // PROXY 服务机到远程主机状态
	};

	static struct ProxyParam
	{
		char Address[256];		// 远程主机地址
		char Cmd[10];		// 远程主机地址
		HANDLE IsConnectedOK;	// PROXY 服务机到远程主机的联结状态
		HANDLE IsExit;	// PROXY 服务机到远程主机的联结状态
		HANDLE ReplaceResponseOK;	// PROXY 服务机到远程主机的联结状态
		HANDLE ResponseOK;	// PROXY 服务机到远程主机的联结状态
		bool CancelReplaceResponse;
		bool cancelResponse;
		bool replaceRequest;
		ProxySockets * pPair;	// 维护一组SOCKET的指针
		void * pProxy;	// 维护一组SOCKET的指针
		int Port;			// 用来联结远程主机的端口
		char Request[MAXBUFFERSIZE];
		char* Response;
		bool isHttps;
		int retryCount;
	}; //结构用来PROXY SERVER与远程主机的信息交换

	SOCKET listentsocket; //用来侦听的SOCKET

	int StartProxyServer() //启动服务
	{
		WSADATA wsaData;
		sockaddr_in local;
		//	SOCKET listentsocket;

		//Log("启动代理服务器");
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		{
			//Log("\nError in Startup session.\n");
			WSACleanup();
			return -1;
		}

		local.sin_family = AF_INET;
		//char ip[100];
		local.sin_addr.s_addr = INADDR_ANY;
		local.sin_port = htons(pport);
		//
		listentsocket = socket(AF_INET,SOCK_STREAM,0);
		//Log("创建侦听套接字");

		if(listentsocket == INVALID_SOCKET)
		{
			//Log("\nError in New a Socket.");
			WSACleanup();
			return -2;
		}

		//Log("绑定侦听套接字");
		if(::bind(listentsocket,(sockaddr *)&local,sizeof(local))!=0)
		{
			//Log("\n Error in Binding socket.");
			WSACleanup();
			return -3;	
		}

		//Log("建立侦听");
		if(::listen(listentsocket,100)!=0)
		{
			//Log("\n Error in Listen.");
			WSACleanup(); 
			return -4;
		}
		//	::listentsocket=listentsocket; 
		//InitializeMemPool();
		//Log("启动侦听线程");
		HANDLE threadhandle = (HANDLE)_beginthreadex(NULL, 0, UserToProxy, (LPVOID)this, NULL, NULL);
		CloseHandle(threadhandle);
		InitializeCriticalSection(&cs);
		return 1;
	}

	static void GetIP(char *ip, const char* hostName){
		BYTE   *p;   
		struct   hostent   *hp; 
		//char   ip[16]; 

		if((hp   =gethostbyname(hostName))!=0) 
		{ 
			p   =(BYTE   *)hp-> h_addr;   
			sprintf(ip,   "%d.%d.%d.%d ",   p[0],   p[1],   p[2],   p[3]); 
			cout<<ip<<"\n";
		} 
	}

	static const char* GetLocalIP(char* ip){
		char   temp[100];
		if(gethostname(temp, sizeof(temp))==0){
			GetIP(ip, temp);
		}
		return ip;
	}


	int CloseServer() //关闭服务
	{
		closesocket(listentsocket);
		//	WSACleanup();
		return 1;
	}

	~MelodyProxy(){
		closesocket(listentsocket);
		DeleteCriticalSection(&cs);
	}
	//分析接收到的字符，得到远程主机地址
	static int ResolveInformation( char * str, char* command, char *address, int * port)
	{
		char buf[MAXBUFFERSIZE], proto[128], *p=NULL;

		//CString strLog;

		sscanf(str,"%s%s%s",command,buf,proto);
		//strcpy(url, buf);
		p=strstr(buf,HTTP);

		//HTTP
		if(p)
		{
			UINT i;
			p+=strlen(HTTP);
			int j=0;
			for(i=0; i<strlen(p); i++)
			{
				if(*(p+i)==':') j=i;
				if( *(p+i)=='/') break;
			}

			*(p+i)=0;
			strcpy(address,p);
			if(j!=0){
				*port=atoi(p+j);
			}
			else{
				*port=80;	//缺省的 http 端口
			}
			p = strstr(str,HTTP);

			//去掉远程主机名: 
			//GET http://www.njust.edu.cn/ HTTP1.1  == > GET / HTTP1.1
			/*for(UINT j=0;j< i+strlen(HTTP);j++)
			{
				*(p+j)=' ';
			}*/

			return 1;
		}
		strcpy(buf,str);
		p=strstr(buf,HTTPS);
		if(p)
		{
			UINT i;
			p+=strlen(HTTPS);
			int j=0;
			for(i=0; i<strlen(p); i++)
			{
				if(*(p+i)==':') j=i;
				if( *(p+i)==' ') break;
			}
			*(p+i)=0;
			strcpy(address,p);
			if(j!=0){
				*port=atoi(p+j+1);
				*(address+j)=0;
			}
			else{
				*port=443;	//缺省的 http 端口
			}
			p = strstr(str,address);

			return 1;
		}

		return 0; 
	}

	static int readContentLength(char* buffer){
		char* p;
		if(p=strstr(buffer,"Content-Length: ")){
			char temp1[100];
			p+=strlen("Content-Length: ");
			int l=strlen(p);
			if(l>30)l=30;
			strncpy(temp1,p,l);
			for(char* i=temp1;i<temp1+30;++i){
				if(i[0]=='\r'){
					i[0]=0;
					break;
				}
			}
			int length=atoi(temp1);
			return length;
		}
		else{
			return -1;
		}
	}
	static wstring GetIEProxy(const std::wstring& strHostName)
    {
		if(proxyMap.find(strHostName) != proxyMap.end()){
			return proxyMap[strHostName];
		}
        std::wstring strRet_cswuyg;
        WINHTTP_AUTOPROXY_OPTIONS autoProxyOptions = {0};
        WINHTTP_CURRENT_USER_IE_PROXY_CONFIG ieProxyConfig = {0};
        BOOL bAutoDetect = FALSE; //“自动检测设置”，但有时候即便选择上也会返回0，所以需要根据url判断
        if(::WinHttpGetIEProxyConfigForCurrentUser(&ieProxyConfig))
        {
            if(ieProxyConfig.fAutoDetect)
            {
                bAutoDetect = TRUE;
            }
            if( ieProxyConfig.lpszAutoConfigUrl != NULL )
            {
                bAutoDetect = TRUE;
                autoProxyOptions.lpszAutoConfigUrl = ieProxyConfig.lpszAutoConfigUrl;
            }
        }
        else
        {
            // error
            return strRet_cswuyg;
        }

        if(bAutoDetect)
        {
            if (autoProxyOptions.lpszAutoConfigUrl != NULL)
            { 
                autoProxyOptions.dwFlags = WINHTTP_AUTOPROXY_CONFIG_URL;
            }
            else
            {
                autoProxyOptions.dwFlags = WINHTTP_AUTOPROXY_AUTO_DETECT;
                autoProxyOptions.dwAutoDetectFlags = WINHTTP_AUTO_DETECT_TYPE_DHCP | WINHTTP_AUTO_DETECT_TYPE_DNS_A;
            }
            autoProxyOptions.fAutoLogonIfChallenged = TRUE;
            HINTERNET hSession = ::WinHttpOpen(0, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, 
                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, WINHTTP_FLAG_ASYNC);
            if (hSession != NULL)
            {
                WINHTTP_PROXY_INFO autoProxyInfo = {0};
                bAutoDetect = ::WinHttpGetProxyForUrl(hSession, strHostName.c_str(), &autoProxyOptions, &autoProxyInfo);
				if(autoProxyInfo.lpszProxy!=NULL){
					strRet_cswuyg=autoProxyInfo.lpszProxy;
					proxyMap[strHostName]=strRet_cswuyg;
				}
				else{
					proxyMap[strHostName]=L"";
				}
                if (hSession!= NULL) 
                {
                    ::WinHttpCloseHandle(hSession);
                }
                if(autoProxyInfo.lpszProxy)
                {
                    if (autoProxyInfo.lpszProxyBypass == NULL)
                    {
                        std::wstring strProxyAddr = autoProxyInfo.lpszProxy;
                        //strRet_cswuyg = GetProxyFromString(eProxyType, strProxyAddr);
                    }
                    if(autoProxyInfo.lpszProxy != NULL)
                    {
                        GlobalFree(autoProxyInfo.lpszProxy);
                    }
                    if(autoProxyInfo.lpszProxyBypass !=NULL) 
                    {
                        GlobalFree(autoProxyInfo.lpszProxyBypass); 
                    }
                }
            }
        }
        else
        {
            if(ieProxyConfig.lpszProxy != NULL)
            {
                if(ieProxyConfig.lpszProxyBypass == NULL)
                {
                    std::wstring strProxyAddr = ieProxyConfig.lpszProxy;
					proxyMap[strHostName]=strRet_cswuyg;
                    //strRet_cswuyg = GetProxyFromString(eProxyType, strProxyAddr);
                }
            }
        }

        if(ieProxyConfig.lpszAutoConfigUrl != NULL)
        {
            ::GlobalFree(ieProxyConfig.lpszAutoConfigUrl);
        }
        if(ieProxyConfig.lpszProxy != NULL)
        {
            ::GlobalFree(ieProxyConfig.lpszProxy);
        }
        if(ieProxyConfig.lpszProxyBypass != NULL)
        {
            ::GlobalFree(ieProxyConfig.lpszProxyBypass);
        }

        return strRet_cswuyg;
    }
	// 取到本地的数据，发往远程主机
	static unsigned int WINAPI UserToProxy(void *pParam)
	{
		sockaddr_in from;
		MelodyProxy* proxy=(MelodyProxy *)pParam;
		int  fromlen = sizeof(from);
		bool isAccept=false;
		char Buffer[MAXBUFFERSIZE];
		int  Len;
		int count=0;

		SOCKET ProxyToUserSocket;
		ProxySockets SPair;
		ProxyParam   ProxyP;
		ZeroMemory(&ProxyP, sizeof(ProxyParam));
		ZeroMemory(&SPair, sizeof(ProxySockets));

		HANDLE pChildThread;
		int retval;
		//Log("正在侦听用户连接");
		ProxyToUserSocket = accept(proxy->listentsocket,(struct sockaddr*)&from,&fromlen);
		//Log("接受连接");
		HANDLE threadhandle = (HANDLE)_beginthreadex(NULL, 0, UserToProxy, (LPVOID)pParam, NULL, NULL);
		CloseHandle(threadhandle);
		if( ProxyToUserSocket==INVALID_SOCKET)
		{ 
			return -5;
		}

		//读客户的第一行数据
		SPair.IsProxyToUserClosed = FALSE;
		SPair.IsProxyToServClosed = TRUE ;
		SPair.ProxyToUserSocket = ProxyToUserSocket;

		//Log("接收用户数据");
		retval = recv(SPair.ProxyToUserSocket,Buffer,sizeof(Buffer),0);

		if(retval==SOCKET_ERROR)
		{ 
			//Log("\nError Recv"); 
			if(SPair.IsProxyToUserClosed == FALSE)
			{
				closesocket(SPair.ProxyToUserSocket);
				SPair.IsProxyToUserClosed = TRUE;
				return 0;
			}
		}

		if(retval==0)
		{
			//Log("Client Close connection\n");
			if(SPair.IsProxyToUserClosed==FALSE)
			{	
				closesocket(SPair.ProxyToUserSocket);
				SPair.IsProxyToUserClosed=TRUE;
				return 0;
			}
		}
		Len = retval;

		//
		SPair.IsProxyToUserClosed = FALSE;
		SPair.IsProxyToServClosed = TRUE;
		SPair.ProxyToUserSocket = ProxyToUserSocket;
		ProxyP.pPair=&SPair;
		ProxyP.IsConnectedOK = CreateEvent(NULL,FALSE,FALSE,NULL);
		ProxyP.ReplaceResponseOK = CreateEvent(NULL,TRUE,FALSE,NULL);
		ProxyP.ResponseOK = CreateEvent(NULL,TRUE,FALSE,NULL);
		ProxyP.pProxy=(void *)proxy;
		ProxyP.retryCount=0;
		ZeroMemory(ProxyP.Request,MAXBUFFERSIZE);
		memcpy(ProxyP.Request,Buffer, Len);

		if(MAXBUFFERSIZE>Len){
			Buffer[Len]=0;
		}
		//Log("分析用户请求");
		bool replaceResponse=false;
		ResolveInformation( Buffer, ProxyP.Cmd, ProxyP.Address, &ProxyP.Port);
		struct hostent *hp=gethostbyname("proxy.tencent.com");
		if(hp!=NULL){
			char url[1000]={0};
			if(ProxyP.Port==443){
				strcpy(url,"https://");
			}
			else{
				strcpy(url,"http://");
			}
			strcat(url,ProxyP.Address);
			CefString s=url;
			CefString proxyStr=GetIEProxy(s.ToWString());
			string ieProxy=proxyStr.ToString();
			int index=ieProxy.find_last_of(":");
			if(index!=-1){
				ProxyP.Port=atoi(ieProxy.substr(index+1).c_str());
				strcpy(ProxyP.Address,ieProxy.substr(0,index).c_str());
			}
			else{
				strcpy(ProxyP.Address,"proxy.tencent.com");
				ProxyP.Port=8080;
			}
		}
		char* bufferAll=new char[MAXBUFFERSIZE*50];
		char* header=new char[MAXBUFFERSIZE];
		int headerLength=0;
		int length=0;
		if(proxy->agentRequest){
			//如果为post请求，读取完所有数据再处理
			if(!strcmp(ProxyP.Cmd,"POST")){
				length=readContentLength(Buffer);
				char* start=strstr(Buffer,"\r\n\r\n");
				headerLength=start-Buffer+4;
				count=Len-headerLength;
				memcpy(bufferAll,start+4,count);
				memcpy(header,Buffer,headerLength);
				header[headerLength]=0;
				while(count<length){
					//Log("从用户地址接收数据");
					retval = recv(SPair.ProxyToUserSocket,Buffer,sizeof(Buffer),0);

					if(retval==SOCKET_ERROR)
					{
						//Log("\nError Recv"); 
						if(SPair.IsProxyToUserClosed==FALSE)
						{
							SPair.IsProxyToUserClosed=TRUE;
							closesocket(SPair.ProxyToUserSocket);
						}
						continue;
					}

					if(retval==0)
					{
						//Log("Client Close connection\n");
						if(SPair.IsProxyToUserClosed==FALSE)
						{
							closesocket(SPair.ProxyToUserSocket);
							SPair.IsProxyToUserClosed=TRUE;
						}
						break;
					}
					Buffer[retval]=0;
					memcpy(bufferAll+count,Buffer,retval);
					count+=retval;
				}
				bufferAll[length]=0;
				proxy->winHandler->agentRequest(header,bufferAll,&ProxyP);
			}
			else{
				proxy->winHandler->agentRequest(Buffer,"",&ProxyP);
			}
			::WaitForSingleObject(ProxyP.ResponseOK,INFINITE);
			if(ProxyP.replaceRequest){
				strcpy(Buffer, ProxyP.Request);
				Len=strlen(Buffer);
			}
			else if(!ProxyP.cancelResponse){
				retval = send(ProxyP.pPair->ProxyToUserSocket,ProxyP.Response,strlen(ProxyP.Response),0);
				if(retval==SOCKET_ERROR)
				{ 
					if(SPair.IsProxyToUserClosed == FALSE)
					{
						SPair.IsProxyToUserClosed = TRUE;
						closesocket(SPair.ProxyToUserSocket);
					}
					return 0;
				}
				replaceResponse=true;
			}
		}
		if(ProxyP.Port==443){
			ProxyP.isHttps=true;
		}
		else{
			ProxyP.isHttps=false;
		}
		pChildThread = (HANDLE)_beginthreadex(NULL, 0, ProxyToServer, (LPVOID)&ProxyP, 0, NULL);

		//Log("等待连接目标地址事件");
		::WaitForSingleObject(ProxyP.IsConnectedOK,60000);
		::CloseHandle(ProxyP.IsConnectedOK);
		bool first=true;

		while(SPair.IsProxyToServClosed == FALSE && SPair.IsProxyToUserClosed == FALSE)
		{	
			//Log("向目标地址发送数据");
			//如果是https，第一次不用发送
			if(first){
				//非htts而且也没有替换
				if(!ProxyP.isHttps&&!replaceResponse){
					//post请求
					if(length){
						retval = send(SPair.ProxyToServSocket,header,headerLength,0);
						retval = send(SPair.ProxyToServSocket,bufferAll,length,0);
					}
					else{
						retval = send(SPair.ProxyToServSocket,Buffer,Len,0);
					}
				}
			}
			else{
				if(proxy->agentRequest){
					//如果为post请求，读取完所有数据再处理
					length=0;
					if(strstr(Buffer,"POST")==Buffer){
						length=readContentLength(Buffer);
						char* start=strstr(Buffer,"\r\n\r\n");
						headerLength=start-Buffer+4;
						count=Len-headerLength;
						memcpy(bufferAll,start+4,count);
						memcpy(header,Buffer,headerLength);
						header[headerLength]=0;
						while(count<length){
							//Log("从用户地址接收数据");
							retval = recv(SPair.ProxyToUserSocket,Buffer,sizeof(Buffer),0);

							if(retval==SOCKET_ERROR)
							{
								//Log("\nError Recv"); 
								if(SPair.IsProxyToUserClosed==FALSE)
								{
									SPair.IsProxyToUserClosed=TRUE;
									closesocket(SPair.ProxyToUserSocket);
								}
								continue;
							}

							if(retval==0)
							{
								//Log("Client Close connection\n");
								if(SPair.IsProxyToUserClosed==FALSE)
								{
									closesocket(SPair.ProxyToUserSocket);
									SPair.IsProxyToUserClosed=TRUE;
								}
								break;
							}
							memcpy(bufferAll+count,Buffer,retval);
							count+=retval;
						}
						bufferAll[length]=0;
						proxy->winHandler->agentRequest(header,bufferAll,&ProxyP);
					}
					else{
						proxy->winHandler->agentRequest(Buffer,"",&ProxyP);
					}
					::WaitForSingleObject(ProxyP.ResponseOK,INFINITE);
					if(ProxyP.replaceRequest){
						strcpy(Buffer, ProxyP.Request);
						Len=strlen(Buffer);
						retval = send(SPair.ProxyToServSocket,Buffer,Len,0);
						if(strstr(Buffer,"GET ")==Buffer||strstr(Buffer,"POST ")==Buffer){
							memcpy(ProxyP.Request, Buffer, Len);
							ProxyP.Request[Len]=0;
						}
					}
					else if(ProxyP.cancelResponse){
						if(length){
							retval = send(SPair.ProxyToServSocket,header,headerLength,0);
							retval = send(SPair.ProxyToServSocket,bufferAll,length,0);
							memcpy(ProxyP.Request,header, headerLength);
							ProxyP.Request[headerLength]=0;
						}
						else{
							retval = send(SPair.ProxyToServSocket,Buffer,Len,0);
							if(strstr(Buffer,"GET ")==Buffer||strstr(Buffer,"POST ")==Buffer){
								memcpy(ProxyP.Request, Buffer, Len);
								ProxyP.Request[Len]=0;
							}
						}
					}
					else{
						retval = send(ProxyP.pPair->ProxyToUserSocket,ProxyP.Response,strlen(ProxyP.Response),0);
						if(retval==SOCKET_ERROR)
						{ 
							if(SPair.IsProxyToUserClosed == FALSE)
							{
								SPair.IsProxyToUserClosed = TRUE;
								closesocket(SPair.ProxyToUserSocket);
							}
							break;
						}
						retval=0;
					}
				}
				else{
					retval = send(SPair.ProxyToServSocket,Buffer,Len,0);
					if(strstr(Buffer,"GET ")==Buffer||strstr(Buffer,"POST ")==Buffer){
						memcpy(ProxyP.Request, Buffer, Len);
						ProxyP.Request[Len]=0;
					}
				}
			}
			if(retval==SOCKET_ERROR)
			{ 
				if(SPair.IsProxyToServClosed == FALSE)
				{
					SPair.IsProxyToServClosed = TRUE;
					closesocket(SPair.ProxyToServSocket);
				}
				break;
			}
			first=false;

			//Log("从用户地址接收数据");
			retval = recv(SPair.ProxyToUserSocket,Buffer,sizeof(Buffer),0);

			if(retval==SOCKET_ERROR)
			{
				//Log("\nError Recv"); 
				if(SPair.IsProxyToUserClosed==FALSE)
				{
					SPair.IsProxyToUserClosed=TRUE;
					closesocket(SPair.ProxyToUserSocket);
				}
				continue;
			}

			if(retval==0)
			{
				//Log("Client Close connection\n");
				if(SPair.IsProxyToUserClosed==FALSE)
				{
					closesocket(SPair.ProxyToUserSocket);
					SPair.IsProxyToUserClosed=TRUE;
				}
				break;
			}
			Len=retval;
			Buffer[Len]=0;
		}
		//End While
		delete bufferAll;
		delete header;
		if(SPair.IsProxyToServClosed == FALSE)
		{
			closesocket(SPair.ProxyToServSocket);
			SPair.IsProxyToServClosed=TRUE;
		}
		if(SPair.IsProxyToUserClosed == FALSE)
		{
			closesocket(SPair.ProxyToUserSocket);
			SPair.IsProxyToUserClosed=TRUE;
		}
		::WaitForSingleObject(pChildThread,60);
		CloseHandle(pChildThread);
		CloseHandle(ProxyP.ResponseOK);
		return 0;
	}

	static int readChunk(char* str, int len, char** pBuffer, MelodyProxy* pProxy){
		if(len==0){
			return 0;
		}
		char* buffer=*pBuffer;
		char temp1[10]={"0x"};
		int res=len;//计算剩余字节数
		char* seek=strstr(str,"\r\n");//查找16进制字符串后面的CRLF
		if(!seek){
			MessageBoxA(NULL,"eror","error",0);
			return 0;
		}
		int l1=seek-str;//16进制字符串长度
		strncpy(temp1+2,str,l1);//构造16进制字符串
		temp1[l1+2]=0;//补0
		char* strEnd;
		//pProxy->winHandler->agentResponse(str);
		int i = (int)strtol(temp1, &strEnd, 16);//十六进制
		res-=l1+2;
		if(i==0){
			return -1;
		}
		if(res<=0){
			return i;
		}
		//不足一个chunk包
		if(res<i){
			//复制数据到content中
			memcpy(buffer,seek+2,res);//复制到缓冲区中
			buffer[res]=0;
			*pBuffer=buffer+res;
			return i-res;//返回剩余的chunk数据的长度
		}
		else{
			memcpy(buffer,seek+2,i);//复制到缓冲区中
			//下一个chunk的指针，剩余字符串长度，重新计算缓存区起始位置
			buffer[i]=0;
			*pBuffer=buffer+i;
			res-=i+2;
			if(res>0){
				return readChunk(seek+2+i+2,res,pBuffer,pProxy);
			}
			else{
				return 0;
			}
		}
	}
	static char *replace(char *st, char *orig, char *repl, char* buffer, int l) {
		ZeroMemory(buffer,l);
		char *ch;
		if (!(ch = strstr(st, orig)))
			return st;
		strncpy(buffer, st, ch-st);  
		buffer[ch-st] = 0;
		sprintf(buffer+(ch-st), "%s%s", repl, ch+strlen(orig));
		return buffer;
	}
	static unsigned int WINAPI ProxyToServer(LPVOID pParam);
	static int Response(char* header, char* content, int count, MelodyProxy* pProxy, ProxyParam* pPar);
};
#endif
