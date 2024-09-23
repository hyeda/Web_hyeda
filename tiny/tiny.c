/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 * GET method to serve static and dynamic content
 */
#include "csapp.h" // 컴씨에서 제공하는 유틸리티 함수들이 정의된 헤더 파일, 소켓 프로그래밍과 관련된 여러 함수들이 포함되어 있음.

void doit(int fd);   // 클라이언트의 요청을 처리하는 핵심 함수.
void read_requesthdrs(rio_t *rp);   // 클라이언트로부터 받은 HTTP 요청 헤더를 읽는다.
int parse_uri(char *uri, char *filename, char *cgiargs);  // 클라이언트 요청 URI를 분석하여, 정적 또는 동적 콘텐츠 요청을 구분함.
void serve_static(int fd, char *filename, int filesize, char *method);  // 정적 파일을 클라이언트에게 제공하는 함수.
void get_filetype(char *filename, char *filetype);  // 요청된 파일의 MIME 타입을 결정하는 함수.
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method);  // 동적 콘텐츠를 실행하여 클라이언트에게 제공하는 함수.
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);   // 클라이언트의 요청이 잘못 됐을 때 오류 메세지를 보냄.



/* Tiny라는 간단한 웹 서버의 메인 루틴이다. 이 서버는 클라이언트로부터 연결 요청을 받고, 요청을 처리한 후 응답을 보내는 기본적인 수행을 한다.
 * 이 프로그램은 포트 번호를 *명령줄 인수*로 받는다. */
int main(int argc, char **argv)  //argc는 인수의 개수, argv는 인수의 배열 
{
  signal(SIGPIPE,SIG_IGN);  // 파이프 또는 소켓 통신에서 발생할 수 있는 SIGPIPE 시그널을 무시하는 코드.
  
  // 서버가 클라이언트의 연결 요청을 기다리는 리슨 소켓의 파일 디스크립터, 클라이언트가 서버에 연결된 후, 데이터를 주고받기 위한 연결 소켓의 파일 디스크립터
  int listenfd, connfd; 

  char hostname[MAXLINE], port[MAXLINE];  // 클라이언트의 호스트 이름과 포트 번호를 저장하는 문자열.
  socklen_t clientlen;  // 클라이언트 주소 구조체의 크기. 
  struct sockaddr_storage clientaddr;  // 클라이언트의 주소 정보를 저장하는 구조체.

 // 명령줄 인수로 포트 번호가 제대로 입력됐는지 확인. 정확히 하나(포트번호)의 명령줄 인수가 전달 되어야 한다.
  if (argc != 2) {  // 전달 된 인수중 첫번째 인수는 무조건 프로그램 이름이니까, 실제 전달한 인수는 argc -1 개가 되는 거임. 
    fprintf(stderr, "usage: %s <port>\n", argv[0]);   // 잘못 실행 했을때 , 포트 번호를 명령줄 인수로 전달해야 한다는 것을 설명
    exit(1);   // 비정상 종료하는 함수, 1은 에러, 0은 성공.
  }

  listenfd = Open_listenfd(argv[1]);  // 포트 번호를 사용해 리슨 소켓을 열고, 파일 디스크립터를 반환.
  while (1) {   // 서버는 무한루프에 진입하여 계속해서 클라이언트의 연결 요청을 기다린다.
    clientlen = sizeof(clientaddr);

    // accept 함수 : 클라이언트의 연결 요청을 기다리다가 요청이 오면 수락하고, 새로운 소켓 fd를 반환 
    // 서버가 요청 수신하는데 사용되는 리슨 소켓의 fd // 클라이언트의 소켓 주소를 저장할 구조체 // 구조체의 크기를 나타내는 변수의 주소.
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  

    // 클라이언트의 소켓 주소 정보를 사람이 읽을 수 있는 형식으로 변환하는 함수, IP주소와 포트 번호를 문자열로 반환 해줌.
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);  // 마지막 0 : 플러그를 설정하지 않기 위해 0을 사용함.

    printf("Accepted connection from (%s, %s)\n", hostname, port);  // 연결된 클라이언트의 호스트 이름과 포트 번호를 출력함.
    doit(connfd);  // 이 함수가 HTTP 요청을 읽고, 정적or 동적 콘텐츠를 클라이언트에 응답함.
    Close(connfd);  // 클라이언트와의 연결이 끝나면 연결 소켓을 닫음. 이후 새로운 클라이언트 연결 요청을 기다림.
  }
}

/* 클라이언트가 보낸 HTTP요청을 분석하고, 요청에 따라 정적 또는 동적 콘텐츠를 반환하는 역할을 한다. */
void doit(int fd)
{
  int is_static;  // 클라이언트가 요청한 내용이 정적인지, 동적인지 나타내는 *플래그 변수*.
  struct stat sbuf;   // 파일 정보(파일 크기, 권한)를 저장하는 struct stat 구조체.

  // 클라이언트로부터 읽은 HTTP 요청 라인을 저장하는 버퍼, HTTP 요청 라인에서 요청방식, URI, 버전 정보를 각각 저장.
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE]; 

  char filename[MAXLINE], cgiargs[MAXLINE];   // 클라이언트가 요청한 파일이름, CGI 프로그램 인수를 저장하는 변수.
  rio_t rio;  // *robust I/O*에서 사용하는 읽기 *버퍼* 구조체.

  Rio_readinitb(&rio, fd);  // fd와 연결된 읽기 버퍼를 초기화(init) 함.
  Rio_readlineb(&rio, buf, MAXLINE);  // 클라이언트로부터 한 줄의 요청 라인을 읽어옴. 결과는 buf에 저장됨.
  printf("Request headers:\n");   // 프로그램이 HTTP 요청 헤더를 처리 중이라는 것을 개발자에게 알리는 역할. 콘솔에 출력됨.
  printf("%s", buf);  // 버퍼에 저장된 데이터를 출력한다. 여기서 buf는 요청 라인임. (HTTP 메소드, URI, 버전정보)
  sscanf(buf, "%s %s %s", method, uri, version);  // 요청라인에서 각각 추출해서 저장함.

  // 클라이언트의 요청 메소드가 GET인지, HEAD인지 비교함.
  if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD")) {
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");   // GET, HEAD이외의 메소드를 사용하면 클라이언트에게 메세지를 보냄.
    return;
  }
  read_requesthdrs(&rio);   // 추가적인 요청 헤더를 처리하는 함수, 클라이언트가 어떤 요청을 했는지 확인하기 위해 읽음.
  
  // URI 파싱 : 클라이언트가 요청한 URI를 분석하여, 요청된 리소스가 정적인지 동적인지 결정함.
  // 정적 콘텐츠라면 filename에 요청된 파일 경로 저장, is_static = 1로 설정
  // 동적 콘텐츠라면 CGI인자들을 cgiargs에 저장하고, is_static = 0으로 설정
  is_static = parse_uri(uri, filename, cgiargs);  

  if (stat(filename, &sbuf) < 0) {  // stat : 요청된 파일이 실제로 존재하는지 확인.
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }

  if (is_static) {  // 정적 콘텐츠 처리
    
    // 파일이 일반 파일인지 확인(파일 타입) , // 소유자가 읽기 권한을 가지고 있는지 확인(읽기 권한)
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {  // 둘 중 하나라도 아니면 파일을 제공할 수 없음.
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size, method);   // 파일이 일반 파일이고, 읽기 권한이 있다면 이 함수를 통해 파일을 클라이언트로 전송.
  }
  else {  // 동적 콘텐츠 처리

    // CGI 프로그램이 실행 파일, // 실행권한이 있는지
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) { 
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs, method);   // 실행 권한이 있다면, 이 함수를 통해 CGI 프로그램을 실행하고, 결과를 클라이언트에 전송.
  }
}

/* 웹 서버에서 클라이언트가 잘못된 요청을 보냈을 때, 서버가 요청을 처리 할 수 없을 때 발생하는 에러를 처리하는 역할
 * 에러 메세지를 생성하고, 그 내용을 클라이언트에게 HTTP 응답으로 전달함. */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
// fd : 클라이언트와 연결을 나타내는 fd, cause: 에러의 원인을 설명하는 문자열, errnum : 에러코드
// 짧은 에러 상태 설명, 긴 에러 메세지는 자세한 설명을 제공
{
  char buf[MAXLINE], body[MAXBUF];

  // sprintf: 문자열을 포맷화해서 버퍼에 저장하는 함수, body: 에러 페이지의 HTML 코드가 저장될 버퍼.
  // 각 sprintf 호출은 body 버퍼에 HTML 페이지의 다른 부분을 추가.
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);   // 배경 색 흰색
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  // HTTP 응답 헤더 생성하고 클라이언트로 전송
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);   // 예를 들어 HTTP/1.0 404 Not Found
  Rio_writen(fd, buf, strlen(buf));   // 위에서 생성된 문자열들을 클라이언트로 전송한다.
  sprintf(buf, "Content-type: text/html\r\n");  // 클라이언트가 응답이 HTML 형식임을 알 수 있게 함.
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));  // 응답 본문(HTML)의 길이를 설정.
  Rio_writen(fd, buf, strlen(buf));

  Rio_writen(fd, body, strlen(body));   // 마지막으로 HTML 형식으로 작성된 에러 페이지를 클라이언트에게 전송함.
}

/* rp(rio_t구조체)를 이용해 rio_readlineb 함수를 통해 HTTP 요청 헤더들을 읽고 출력하는 역할.
 * 요청 라인은 클라이언트가 서버에 무엇을 요청하는지 설명하고, 헤더들은 추가적인 정보를 제공한다. */
void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];  // 버퍼: 한 줄씩 읽어 저장할 공간.

  Rio_readlineb(rp, buf, MAXLINE);  // 요청라인 첫줄은 Host내용이라 HTTP 1.0에선 필요없어서 프린트 안함.

  while(strcmp(buf, "\r\n")) {  // '\r\n'(빈줄 : 헤더의 끝)이 나올 때까지 계속 요청 헤더를 읽음. -> 같아지면 0이 반환되서 false
    Rio_readlineb(rp, buf, MAXLINE);  // 한 줄 씩 읽고
    printf("%s", buf);  // 읽은 헤더를 출력.
  }
  return;
}

/* 클라이언트가 요청한 *URI*를 분석해서 정적 콘텐츠인지, 동적 콘텐츠인지 판단한 후
 * 적절한 파일 이름과 CGI 인자를 설정하는 역할을 한다. */ 
int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;  // URI에서 특정 문자를 찾을 때 사용할 포인터 변수.

  if (!strstr(uri, "cgi-bin")) {  // URI 문자열에 cgi-bin이 포함되어 있지 않다면 정적 콘텐츠 요청이라고 판단.
    strcpy(cgiargs, "");  // CGI 인자 비우기 (정적 콘텐츠는 인자가 필요 없으므로)
    strcpy(filename, ".");  // 파일 이름의 기본 경로를 현재 디렉터리를 의미하는 '.'을 넣고 
    strcat(filename, uri);  // 그 뒤에 uri를 이어 붙여서(strcat) 파일 경로를 만든다.
    if (uri[strlen(uri)-1] == '/')  // URI가 '/'로 끝나면 디폴트 파일인 'home.html'로 설정한다.
      strcat(filename, "home.html");
    return 1;  // 정적 콘텐츠를 나타냄.
  }
  else {  // cgi-bin이 포함되어 있다면 동적 콘텐츠.
    ptr = index(uri, '?');  // '?' 문자를 찾아서 CGI 인자를 분리.
    if (ptr) {  // NULL이 아니라면 유효한 주소를 가리키고 있으므로 '참'으로 평가 됨.
      strcpy(cgiargs, ptr+1);   // '?' 이후의 부분을 cgiargs에 복사 (CGI 인자).
      *ptr = '\0';  // '?' 문자를 NULL로 대체해서 filename을 끝냄.
    }
    else  
      strcpy(cgiargs, "");  // '?'가 없으면 CGI 인자는 없으므로 빈 문자열로 설정함.
    strcpy(filename, ".");  // 현재 디렉토리로 시작.
    strcat(filename, uri);  // URI를 filename에 이어 붙임.
    return 0;   // 동적 콘텐츠를 나타냄.
  }
}

/* 정적 콘텐츠(디스크에 저장된 파일)를 클라이언트에게 제공하는 역할.
 * 클라이언트가 요청한 파일을 읽어서 그 내용을 클라이언트에게 전송한다. */
void serve_static(int fd, char *filename, int filesize, char *method)   // fd에 데이터를 보내는게 클라이언트에게 응답을 보내는 것과 동일.
{
  int srcfd;  // 요청한 파일을 열 때 반환되는 파일 디스크립터를 저장하는 변수.
  char *srcp, filetype[MAXLINE], buf[MAXBUF];   // 요청한 파일을 메모리 매핑한 후 반환되는 메모리 영역의 시작 주소.

  get_filetype(filename, filetype);   // 파일의 확장자에 따라 MIME 타입을 결정함. 

  // 클라이언트에게 보낼 HTTP 응답 헤더를 생성하는 코드
  sprintf(buf, "HTTP/1.0 200 OK\r\n");  // 상태 코드.
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);   // 서버 정보
  sprintf(buf, "%sConnection: close\r\n", buf);   // 연결 종료 알림.
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);  // 콘텐츠 길이.
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);  // MIME 타입

  Rio_writen(fd, buf, strlen(buf));   // 위에서 생성한 응답 헤더를 클라이언트에게 전송
  printf("Response headers:\n");        
  printf("%s", buf);  // 터미널에 출력해서 어떤 헤더가 전송됐는지 개발자에게 보여줌.

  // method GET일때만 파일 열수 있게 만들기.
  if (!strcasecmp(method, "GET")) {   
    srcfd = Open(filename, O_RDONLY, 0);  // 파일을 (읽기 전용으로) 열고 해당 파일을 식별하는 고유한 정수(fd)를 반환. 
    
    //*메모리 매핑* 방식으로 파일을 읽음. 파일을 메모리에 매핑하면, 해당 파일을 메모리처럼 다룰 수 있어 효율적인 파일 읽기 작업이 가능함.
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);  

    Close(srcfd);   // 파일 디스크립터를 닫고(연결을 종료) 운영체제의 자원을 반환하는 함수.
    Rio_writen(fd, srcp, filesize);   // 매핑한 메모리의 파일 내용을 클라이언트에게 전송.
    Munmap(srcp, filesize);   // 사용한 메모리 매핑을 해제함.
    
    // srcp = (char *)malloc(filesize);  // 메모리 파일 사이즈만큼 할당.
    // Rio_readn(srcfd, srcp, filesize);   // srcfd에서 사이즈바이트 만큼 데이터를 읽어서, srcp에 저장.
    
    // Close(srcfd);   // 파일 디스크립터를 닫고(연결을 종료) 운영체제의 자원을 반환하는 함수.
    // Rio_writen(fd, srcp, filesize);   // 메모리의 파일 내용을 클라이언트에게 전송.
    // free(srcp);   // malloc과 항상 세트
  }
  
}

/* filename의 파일 확장자를 확인하여 그에 맞는 MIME 타입을 filetype에 저장하는 함수. 
 * 파일이 어떤 유형인지 결정해서 클라이언트에게 정확한 파일 타입을 전달할 수 있도록 돕는다. */
void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))  // filename에 .html이 포함 되어 있으면
    strcpy(filetype, "text/html");  // filetype에 text/html 복사해 넣음. = HTML 파일이라는 뜻.
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else if (strstr(filename, ".mp4"))  // 추가했음!! 숙제 문제 11.7
    strcpy(filetype, "video/mp4");
  else  // 하나도 해당되지 않으면
    strcpy(filetype, "text/plain");   // 기본적으로 텍스트 파일로 간주함.
}

/* Tiny 웹 서버가 CGI 프로그램을 실행해서 클라이언트에게 동적 콘텐츠(CGI 프로그램의 출력 결과)를 제공하는 역할. */
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method)
{
  char buf[MAXLINE], *emptylist[] = { NULL };

  // HTTP 응답 헤더를 클라이언트에게 전송하는 코드
  sprintf(buf, "HTTP/1.0 200 OK\r\n");  
  Rio_writen(fd, buf, strlen(buf)); 
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (!strcasecmp(method, "GET")) {
    // 프로세스 분기 *Fork*
    if (Fork() == 0) {  // 자식 프로세스를 의미.
      
      setenv("QUERY_STRING", cgiargs, 1);   
      // 자식 프로세스가 실행 되기 전에 CGI 환경 변수 설정. 이 변수는 CGI 프로그램이 클라이언트로부터 전달받은 쿼리 매개변수를 인식할 수 있도록 하는 역할을 함.
      // 첫번째 인자: 환경 변수의 이름. -> 프로그램에서 이 변수를 사용해서 쿼리 문자열을 읽는다.
      // 두번째 인자: 환경 변수 값 -> 클라이언트가 URI에서 보내온 쿼리 문자열 ex) 15000&213
      // 세번째 인자: 1 -> 기존 값을 덮어쓰겠다. 0 -> 환경 변수를 새로 설정하겠다. 

      Dup2(fd, STDOUT_FILENO);  // fd의 디스크립터가 STDOUT_FILENO에 복제 됨. 따라서 자식 프로세스가 표준 출력을 통해 데이터를 쓰면,
      // 이 데이터는 fd가 가리키는 대상(지금은 클라이언트와의 연결 소켓)에 쓰이게 됨.
      // CGI 프로그램이 출력하는 모든 내용이 클라이언트로 직접 전달됩니다.

      Execve(filename, emptylist, environ);   // 자식 프로세스가 CGI 프로그램(filename)을 실행하게 함. environ(현재 프로세스의 환경 변수 QUERY_STRING)
      // CGI 프로그램을 실행하고, 자식 프로세스의 현재 프로세스를 CGI 프로그램으로 교체함. 
      // CGI 프로그램의 인자는 환경 변수(environ)를 통해 전달되며, 별도의 인자 배열이 필요하지 않습니다. 따라서 emptylist를 사용하여 빈 배열을 전달합니다.
      // Execve 호출 이후에는 현재 프로세스가 CGI 프로그램으로 대체되므로, 자식 프로세스의 나머지 코드는 실행되지 않습니다.
    }
    Wait(NULL);   // 부모 프로세스가 자식 프로세스 종료를 기다리고 종료되면 자식 프로세스를 수거함.
  }
}