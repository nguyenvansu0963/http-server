#include "exp1.h"
#include "exp1lib.h"
#include <sys/wait.h>

// 子プロセスのシグナルハンドラ
void sigChldHandler(int sig);
void printChildProcessStatus(pid_t pid, int status);

#define GET_NUM 0
#define POST_NUM 1
int Request = 0;
char data[1024];

// 構造体定義
typedef struct {
  char cmd[64];
  char path[256];
  char real_path[256];
  char type[64];
  int code;
  int size;
  char location[256]; // リダイレクト先URL
} exp1_info_type;

// HTTPセッション処理関数
int exp1_http_session(int sock);
void exp1_parse_status(char* status, exp1_info_type *pinfo);
int exp1_parse_header(char* buf, int size, exp1_info_type* info);
void exp1_check_file(exp1_info_type *info);
void exp1_http_reply(int sock, exp1_info_type *info);
void exp1_send_404(int sock);
void exp1_send_file(int sock, char* filename);
void post_http(int sock, char *buf, exp1_info_type *info);
void reply_redirect(int sock, char *body, exp1_info_type *info);
void remove_newlines(char *str);

int main(int argc, char **argv)
{
   int sock_listen = exp1_tcp_listen(10046);

  // 子プロセス終了時にのシグナルハンドラを指定
  struct sigaction sa;
  sigaction(SIGCHLD, NULL, &sa);
  sa.sa_handler = sigChldHandler;
  sa.sa_flags = SA_NODEFER;
  sigaction(SIGCHLD, &sa, NULL);

  while(1){
    struct sockaddr_storage addr;
    socklen_t len = sizeof(addr);
    int sock_client = accept(sock_listen, (struct sockaddr *) &addr, &len);
    if (sock_client == -1) {
      if (errno != EINTR) perror("accept");
      continue;
    }

    // 接続元のIP・ポートを表示
    char hbuf[NI_MAXHOST];
    char sbuf[NI_MAXSERV];
    getnameinfo((struct sockaddr *)&addr, len, hbuf, sizeof(hbuf),
                sbuf, sizeof(sbuf), 
                NI_NUMERICHOST | NI_NUMERICSERV);
    fprintf(stderr, "accept: %s:%s\n", hbuf, sbuf);

    //プロセス生成
    pid_t pid = fork();
    if (pid == -1) {
      //エラー処理
      perror("fork");
      close(sock_client);
    } else if (pid == 0) {
      // 子プロセス
      close(sock_listen);//サーバソケットクローズ
      exp1_http_session(sock_client);
      shutdown(sock_client, SHUT_RDWR);//アクセプトソケットクローズ
      close(sock_client);
      _exit(0);
    } else {
      //親プロセス
      close(sock_client);//アクセプトソケットクローズ
    }

    // 子プロセスの終了処理
    int status = -1;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
      // 終了した(かつSIGCHLDでキャッチできなかった)子プロセスが存在する場合 
      // WNOHANGを指定してあるのでノンブロッキング処理
      // 各子プロセス終了時に確実に回収しなくても新規クライアント接続時に回収できれば十分なため．
      printChildProcessStatus(pid, status);
    }
  }
}

// シグナルハンドラによって子プロセスのリソースを回収する
void sigChldHandler(int sig) {
  //子プロセスの終了を待つ
  int status = -1;
  pid_t pid = wait(&status);

  // 非同期シグナルハンドラ内でfprintfを使うことは好ましくないが，
  // ここではプロセスの状態表示に簡単のため使うことにする
  printChildProcessStatus(pid, status);
}

void printChildProcessStatus(pid_t pid, int status) {
  fprintf(stderr, "sig_chld_handler:wait:pid=%d,status=%d\n", pid, status);
  fprintf(stderr, "  WIFEXITED:%d,WEXITSTATUS:%d,WIFSIGNALED:%d,"
          "WTERMSIG:%d,WIFSTOPPED:%d,WSTOPSIG:%d\n",
          WIFEXITED(status), WEXITSTATUS(status), WIFSIGNALED(status),
          WTERMSIG(status), WIFSTOPPED(status), WSTOPSIG(status));
}

int exp1_http_session(int sock)
{
  char buf[2048];
  memset(buf, 0, sizeof(buf));

  int recv_size = 0;
  exp1_info_type info;
  int ret = 0;

  while(ret == 0){
    int size = recv(sock, buf + recv_size, 2048, 0);
    recv_size += size;
    if (size > 0) {
      buf[recv_size] = '\0';  // ここで明示的にnull終端
    }
    if(size == -1){
      return -1;
    }

    ret = exp1_parse_header(buf, recv_size, &info);
  }
  printf("---------\n");
  printf("%s\n",buf);
  printf("---------\n");
  
  if ( Request == GET_NUM) {  //0:GET 1:POST  ここを変えた
    if (strcmp(info.type,"text/php") == 0) {
      reply_redirect(sock, data, &info);
    } else {
      exp1_http_reply(sock, &info);
    }
  } else {
    if (strcmp(info.type,"text/php") == 0) {    //phpならpost_httpに
      post_http(sock, buf, &info);
    } else {
      if (strcmp(info.path,"/get_image") == 0) {  //ベンチマーク(画像)なら/get_imageか調べる
        char *body = NULL;
        for (int i = 0; buf[i + 3] != '\0'; i++) {    //ファイル名抽出 ただしfile=
          if (buf[i] == '\r' && buf[i + 1] == '\n' &&
              buf[i + 2] == '\r' && buf[i + 3] == '\n') {
            body = buf + i + 4;  // 本文の先頭を指す
            break;
          }
        }
        
        if (body) {
          char *eq = strchr(body, '=');
          if (eq && *(eq + 1) != '\0') {
            eq++;  // '='の次からが値
            eq--;  // '/' を挿入する位置
            *eq = '/';  // '=' を '/' に置換
            body = eq;  // 返すポインタを更新
          }
        }

        remove_newlines(body);
        strcpy(info.path,body);
        printf("infopath:%s\n",body);
        exp1_check_file(&info);
      }
      // /get_imageのときは別途処理を追加
      exp1_http_reply(sock, &info);
    }
  }

  

  return 0;
}

int exp1_parse_header(char* buf, int size, exp1_info_type* info)
{
  char status[1024];
  int i, j;

  enum state_type
  {
    PARSE_STATUS,
    PARSE_END
  }state;

  state = PARSE_STATUS;
  j = 0;

  for(i = 0; i < size; i++){
    switch(state){
    case PARSE_STATUS:
      if(buf[i] == '\r'){
        status[j] = '\0';
        j = 0;
        state = PARSE_END;
        exp1_parse_status(status, info);
        //exp1_check_file(info);
      }else{
        status[j] = buf[i];
        j++;
      }
      break;
    }

    if (state == PARSE_END)
    {
      // ★ Authorization ヘッダの処理をここに追加する ↓↓↓
      for (; i < size - 21; i++)
      {
        if (strncmp(buf + i, "Authorization: Basic ", 21) == 0)
        {
          char encoded[256];
          int k = 0;
          i += 21;
          while (i < size && buf[i] != '\r' && k < 255)
          {
            encoded[k++] = buf[i++];
          }
          encoded[k] = '\0';

          // Base64デコード処理（Linuxコマンド使用）
          char tmpfile[L_tmpnam];
          char cmd[1024], decoded[256] = {0};

          tmpnam(tmpfile);
          sprintf(cmd, "echo %s | base64 -d > %s", encoded, tmpfile);
          system(cmd);

          FILE *fp = fopen(tmpfile, "r");
          if (fp)
          {
            fgets(decoded, sizeof(decoded), fp);
            fclose(fp);
          }
          remove(tmpfile);

          // printf("DEBUG: decoded = %s\n", decoded);  // デバッグ表示
          if (strcmp(decoded, "admin:12345") == 0)
          {
            info->code = 200; // 認証成功
          }
          else if (strcmp(decoded, "user:12345") == 0)
          {
            info->code = 403; // 認証成功だが、このユーザにはアクセス権限がない
          }
          else
          {
            info->code = 401; // 認証失敗
          }
          break;
        }
      }

      // ★ 認証結果が info->code に反映された状態でファイルチェックへ
      exp1_check_file(info);

      return 1;
    }
  }

  return 0;
}

void exp1_parse_status(char* status, exp1_info_type *pinfo)
{
  char cmd[1024];
  char path[1024];
  char getpost[5];
  char* pext;
  int i, j;
  int num=0;

  enum state_type
  {
    SEARCH_CMD,
    SEARCH_PATH,
    SEARCH_END
  }state;

  state = SEARCH_CMD;
  j = 0;
  for(i = 0; i < strlen(status); i++){
    switch(state){
    case SEARCH_CMD:
      if(status[i] == ' '){
        cmd[j] = '\0';
        j = 0;
        state = SEARCH_PATH;
      }else{
        cmd[j] = status[i];
        j++;
      }
      break;
    case SEARCH_PATH:
      if(status[i] == ' '){
        path[j] = '\0';
        j = 0;
        state = SEARCH_END;
      }else{
        path[j] = status[i];
        j++;
      }
      break;
    }
  }
  sscanf(status, "%s", getpost);
  char path_only[256];
  //->これでPOSTやGETがgetpostに入る
  if (strcmp(getpost,"GET") == 0) {
    Request = GET_NUM;
    char *qmark_pos = strchr(path, '?');  //?を含む = htmlならこっち
    if (qmark_pos) {
      size_t path_len = qmark_pos - path;
      if (path_len >= sizeof(path_only)) path_len = sizeof(path_only) - 1;
      strncpy(path_only, path, path_len);
      path_only[path_len] = '\0';
      strcpy(path,path_only);

      // クエリは '?' の次の文字列から
      //data = qmark_pos + 1;
      strcpy(data, qmark_pos+1);
    }
    
  } else if (strcmp(getpost, "POST") == 0) {
    Request = POST_NUM;
  }
  strcpy(pinfo->cmd, cmd);
  strcpy(pinfo->path, path);
}

void exp1_check_file(exp1_info_type *info)
{
  struct stat s;
  int ret;
  char* pext;

  if (strcmp(info->path, "/old.html") == 0)
  {
    info->code = 301;
    strcpy(info->location, "/new.html");
    return;
  }
  else if (strcmp(info->path, "/temp.html") == 0)
  {
    info->code = 302;
    strcpy(info->location, "/temporary.html"); // リダイレクト先のパス
    return;
  }
  else if (strcmp(info->path, "/postdone") == 0)
  {
    info->code = 303;
    strcpy(info->location, "/thankyou.html");// リダイレクト先
    return;
  }
  else if (strcmp(info->path, "/secret.html") == 0)
  {
    if (info->code == 401)
    {
      // 認証失敗
      return;
    }
    else if (info->code == 403)
    {
      // 認証成功したが、アクセス権なし
      return;
    }
  }
  else if (strcmp(info->path, "/teapot") == 0)
  {
    info->code = 418;
    return;
  }



  sprintf(info->real_path, "html%s", info->path);
  //printf("asasa:%s%s\n",info->real_path,info->cmd);
  ret = stat(info->real_path, &s);

  if((s.st_mode & S_IFMT) == S_IFDIR){
    sprintf(info->real_path, "%s/index.html", info->real_path);
  }

  ret = stat(info->real_path, &s);

  if(ret == -1){
    info->code = 404;
  }else{
    info->code = 200;
    info->size = (int) s.st_size;
  }

  pext = strstr(info->path, ".");
  if(pext != NULL && strcmp(pext, ".html") == 0){
    strcpy(info->type, "text/html");
  }else if(pext != NULL && strcmp(pext, ".jpg") == 0){
    strcpy(info->type, "image/jpeg");
  }else if(pext != NULL && strcmp(pext, ".php") == 0)   {   //ここを追加
    strcpy(info->type, "text/php");  //ここを追加
  }else if(pext != NULL && strcmp(pext, ".mp4") == 0){
    strcpy(info->type, "video/mp4");
  }else if(pext != NULL && strcmp(pext, ".webm") == 0){
    strcpy(info->type, "video/webm");
  }else if(pext != NULL && strcmp(pext, ".ogg") == 0){
    strcpy(info->type, "video/ogg");
  }

}

void exp1_http_reply(int sock, exp1_info_type *info)
{
  char buf[16384];
  int len;
  int ret;

  if (info->code == 301)
  {
    char buf[1024];
    int len = 0;
    len += sprintf(buf + len, "HTTP/1.0 301 Moved Permanently\r\n");
    len += sprintf(buf + len, "Location: %s\r\n", info->location);
    len += sprintf(buf + len, "Content-Type: text/html\r\n");
    len += sprintf(buf + len, "\r\n");
    len += sprintf(buf + len, "<html><body>Moved to <a href=\"%s\">%s</a></body></html>\n",
                   info->location, info->location);

    send(sock, buf, len, 0);
    return;
  }
  else if (info->code == 302)
  {
    char buf[1024];
    int len = 0;
    len += sprintf(buf + len, "HTTP/1.0 302 Found\r\n");
    len += sprintf(buf + len, "Location: %s\r\n", info->location);
    len += sprintf(buf + len, "Content-Type: text/html\r\n");
    len += sprintf(buf + len, "\r\n");
    len += sprintf(buf + len,
                   "<html><body>Temporarily moved to <a href=\"%s\">%s</a></body></html>\n",
                   info->location, info->location);
    send(sock, buf, len, 0);
    return;
  }
  else if (info->code == 303)
  {
    char buf[1024];
    int len = 0;
    len += sprintf(buf + len, "HTTP/1.0 303 See Other\r\n");
    len += sprintf(buf + len, "Location: %s\r\n", info->location);
    len += sprintf(buf + len, "Content-Type: text/html\r\n");
    len += sprintf(buf + len, "\r\n");
    len += sprintf(buf + len,
                   "<html><body>See other: <a href=\"%s\">%s</a></body></html>\n",
                   info->location, info->location);
    send(sock, buf, len, 0);
    return;
  }
  else if (info->code == 401)
  {
    char buf[1024];
    int len = 0;
    len += sprintf(buf + len, "HTTP/1.0 401 Unauthorized\r\n");
    len += sprintf(buf + len, "WWW-Authenticate: Basic realm=\"Secret Zone\"\r\n");
    len += sprintf(buf + len, "Content-Type: text/html\r\n");
    len += sprintf(buf + len, "\r\n");
    len += sprintf(buf + len,
                   "<html><body><h1>401 Unauthorized</h1><p>You must provide valid credentials.</p></body></html>\n");

    send(sock, buf, len, 0);
    return;
  }
  else if (info->code == 403)
  {
    char buf[1024];
    int len = 0;
    len += sprintf(buf + len, "HTTP/1.0 403 Forbidden\r\n");
    len += sprintf(buf + len, "Content-Type: text/html\r\n");
    len += sprintf(buf + len, "\r\n");
    len += sprintf(buf + len,
                   "<html><body><h1>403 Forbidden</h1><p>You are not allowed to access this resource.</p></body></html>\n");

    send(sock, buf, len, 0);
    return;
  }
  else if (info->code == 404)
  {
    exp1_send_404(sock);
    printf("404 not found %s\n", info->path);
    return;
  }
  else if (info->code == 418)
  {
    char buf[1024];
    int len = 0;
    len += sprintf(buf + len, "HTTP/1.0 418 I'm a teapot\r\n");
    len += sprintf(buf + len, "Content-Type: text/html; charset=UTF-8\r\n"); 
    len += sprintf(buf + len, "\r\n");
    len += sprintf(buf + len,
                   "<html><body><h1>418 I'm a teapot</h1><p>I refuse to brew coffee because I am a teapot ☕.</p></body></html>\n");

    send(sock, buf, len, 0);
    return;
  }

  len = sprintf(buf, "HTTP/1.0 200 OK\r\n");
  len += sprintf(buf + len, "Content-Length: %d\r\n", info->size);
  len += sprintf(buf + len, "Content-Type: %s\r\n", info->type);
  len += sprintf(buf + len, "\r\n");

  ret = send(sock, buf, len, 0);
  if(ret < 0){
    shutdown(sock, SHUT_RDWR);
    close(sock);
    return;
  }

  exp1_send_file(sock, info->real_path);
}

void exp1_send_404(int sock)
{
  char buf[16384];
  int ret;

  sprintf(buf, "HTTP/1.0 404 Not Found\r\n\r\n");
  printf("%s", buf);
  ret = send(sock, buf, strlen(buf), 0);

  if(ret < 0){
    shutdown(sock, SHUT_RDWR);
    close(sock);
  }
}

void exp1_send_file(int sock, char* filename)
{
  FILE *fp;
  int len;
  char buf[16384];

  fp = fopen(filename, "rb");
  if(fp == NULL){
    shutdown(sock, SHUT_RDWR);
    close(sock);
    return;
  }

  len = fread(buf, sizeof(char), 16384, fp);
  while(len > 0){
    int ret = send(sock, buf, len, 0);
    if(ret < 0){
      shutdown(sock, SHUT_RDWR);
      close(sock);
      break;
    }
    len = fread(buf, sizeof(char), 1460, fp);
  }

  fclose(fp);
}

void post_http(int sock, char *buf, exp1_info_type *info) {
    // 
    char *body = NULL;
    for (int i = 0; buf[i + 3] != '\0'; i++) {
      if (buf[i] == '\r' && buf[i + 1] == '\n' &&
          buf[i + 2] == '\r' && buf[i + 3] == '\n') {
        body = buf + i + 4;  // 本文の先頭を指す
        break;
      }
    }

    if (body == NULL) {
      return;
    }

    remove_newlines(body);
    printf("POSTボディ: %s\n", body);

    // 改行を削除したのでbodyには「text1=value1&text2=value2」った形で入る
    reply_redirect(sock, body, info);
}

void reply_redirect(int sock, char *body, exp1_info_type *info) {
    char lenstr[32];
    char path[128];

    // 実行するPHPファイルのパスを構築
    snprintf(path, sizeof(path), "/home/pi/sample/day6/html%s", info->path);
    printf("filepath: %s\n", path);

    snprintf(lenstr, sizeof(lenstr), "%d", strlen(body)); // POSTサイズ

    char *method;
    if (Request == POST_NUM) {
        method = "POST";
    } else {
        method = "GET";
    }


    int in_pipe[2], out_pipe[2];
    pipe(in_pipe);
    pipe(out_pipe);


    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        close(in_pipe[0]); close(in_pipe[1]);
        close(out_pipe[0]); close(out_pipe[1]);
        return;
    } else if (pid == 0) {
        // 子プロセス
        dup2(in_pipe[0], 0);
        dup2(out_pipe[1], 1);
        close(in_pipe[1]); close(out_pipe[0]);

        setenv("REDIRECT_STATUS", "200", 1);
        setenv("REQUEST_METHOD", method, 1);
        setenv("SCRIPT_FILENAME", path, 1);

        if (Request == POST_NUM) {
            setenv("CONTENT_TYPE", "application/x-www-form-urlencoded", 1);
            setenv("CONTENT_LENGTH", lenstr, 1);
        } else {
            setenv("QUERY_STRING", body, 1);
        }

        execlp("php-cgi", "php-cgi", NULL);
        perror("execlp");
        _exit(1);  // ← 異常終了
    } else {
        // 親プロセス
        close(in_pipe[0]); close(out_pipe[1]);

        if (Request == POST_NUM) {
            write(in_pipe[1], body, info->size);
        }
        close(in_pipe[1]);

        send(sock, "HTTP/1.1 200 OK\r\n", 17, 0);
        char buffer[4096];
        ssize_t n;
        while ((n = read(out_pipe[0], buffer, sizeof(buffer))) > 0) {
            send(sock, buffer, n, 0);
        }
        close(out_pipe[0]);

        // 子プロセスを同期で待つ
        int status = -1;
        waitpid(pid, &status, 0);
        printChildProcessStatus(pid, status);
    }

}

//末尾の改行を取り除く関数(\n\rの場合も対応)
void remove_newlines(char *str) {
    int len = strlen(str);
    if (len >= 1 && str[len-1] == '\n') {
      str[len-1] = '\0';
      if (len >= 2 && str[len-2] == '\r') {
        str[len-2] = '\0';
      }
    }
}


