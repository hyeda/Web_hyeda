/* CGI 프로그램, 웹 서버에서 실행 되며 두 숫자를 더하는 간단한 웹 애플리케이션이다. */
#include "csapp.h"

int main(void) {

  char *buf, *p;
  char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
  int n1=0, n2=0;

  // 쿼리 문자열 추출
  if ((buf = getenv("QUERY_STRING")) != NULL) {   // 환경변수를 통해 전달된 URL 쿼리 문자열을 가져옴. ex) 1&2
    p = strchr(buf, '&');   // & 문자를 찾아서 arg1과 arg2로 나누기 위한 포인터를 저장함.
    *p = '\0';  // & 문자를 \0로 대체해서 문자열을 두 부분으로 나눔.
    strcpy(arg1, buf);  // 1
    strcpy(arg2, p+1);  // 2
    
    // 문자열로 저장된 두 값을 정수로 변환
    n1 = atoi(arg1);  
    n2 = atoi(arg2);
  }

  // 응답 본문 생성, content 버퍼에 작성함. 가독성과 유연성을 높이기 위해 문자열을 나눠서 저장.
  sprintf(content, "QUERY_STRING=%s", buf);   // 사실상 이부분은 아래서 덮어씌워지고 있어서 필요없는 부분임. sprintf는 초기화 시키고 새로운 문자열로 덮어씌움.
  sprintf(content, "Welcome to add.com: ");
  sprintf(content, "%sTHE Internet addition portal.\r\n<p>", content);
  sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>", content, n1, n2, n1 + n2);
  sprintf(content, "%sThanks for visiting!\r\n", content);

  // HTTP 응답 생성
  printf("Connection: close\r\n");  // 서버가 클라이언트와의 연결을 끊겠다는 걸 알리는 헤더.
  printf("Content-length: %d\r\n", (int)strlen(content));   // 콘텐츠 길이.
  printf("Content-type: text/html\r\n\r\n");  // MIME 타입을 정의. 여기서는 HTML 이므로 text/html로 지정.
  printf("%s", content);  // 실제로 생성된 HTML 콘텐츠를 출력.

  fflush(stdout);   // 버퍼에 남아있는 데이터를 즉시 출력함. 그렇게 함으로써 표준 출력 스트림의 버퍼를 비움.
  exit(0);  // 프로그램을 정상적으로 종료함.
}
