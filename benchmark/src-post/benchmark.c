#include "exp1.h"
#include "exp1lib.h"
#include <sys/resource.h>

const char* base_path = "";
 
char g_hostname[256];
pthread_mutex_t g_mutex;
int g_error_count;

void exp1_session_error();
void* exp1_eval_thread(void* param);
void randamize_array(int arr[], int size);
void update_rlimit(int resource, int soft, int hard);


int main(int argc, char** argv) {
 
  if(argc != 3){
    printf("usage: %s [ip address] [# of clients]\n", argv[0]);
    exit(-1);
  }
  strcpy(g_hostname, argv[1]);
  int th_num = atoi(argv[2]);
 
  int fileid_arr_size = 100;
  int fileid_arr[fileid_arr_size];
  for ( int i=0; i<fileid_arr_size; i++ ) {
    fileid_arr[i] = i;
  }
  //printf("1\n");
  randamize_array(fileid_arr, fileid_arr_size);
 
  // set resource to unlimited
  //printf("2\n");
  update_rlimit(RLIMIT_STACK, (int)RLIM_INFINITY, (int)RLIM_INFINITY);
   
  g_error_count = 0;
  //printf("3\n");
  pthread_mutex_init(&g_mutex, NULL);
  pthread_t* th = malloc(sizeof(pthread_t) * th_num);
 
  struct timespec ts_start;
  struct timespec ts_end;
  //printf("4\n");
  clock_gettime(CLOCK_MONOTONIC, &ts_start);
 
  //printf("5\n");
  for(int i = 0; i < th_num; i++){
    int *pfileid = malloc(sizeof(int));
    *pfileid = fileid_arr[i%fileid_arr_size];
    pthread_create(&th[i], NULL, exp1_eval_thread, pfileid);
  }
 
  //ここが問題
  //printf("6\n");
  for(int i = 0; i < th_num; i++){
    pthread_join(th[i], NULL);
  }
 
  //printf("7\n");
  clock_gettime(CLOCK_MONOTONIC, &ts_end);
  time_t diffsec = difftime(ts_end.tv_sec, ts_start.tv_sec);
  long diffnanosec = ts_end.tv_nsec - ts_start.tv_nsec;
  double total_time = diffsec + diffnanosec*1e-9;
 
  //printf("8\n");
  printf("total time is %10.10f\n", total_time); 
  printf("session error ratio is %1.3f\n",
  (double)g_error_count / (double) th_num);
 
  free(th);
}


void randamize_array(int arr[], int size) {
  srand(time(NULL));
  for ( int i=0; i<size; i++ ) {
    int temp = arr[i];
    int r = rand() % size;
      arr[i] = arr[r];
      arr[r] = temp;
  }
}

void update_rlimit(int resource, int soft, int hard) {
  struct rlimit rl;
  getrlimit(resource, &rl);
  rl.rlim_cur = soft;
  rl.rlim_max = hard;
  if (setrlimit(resource, &rl ) ==  -1 ) {
    perror("setrlimit");
    exit(-1);
  }
}


 void* exp1_eval_thread(void* param)
 {
   int sock;
   int* pfileid;
   int fileid;
   char command[1024];
   char buf[2048];
   int total;
   int ret;
 
   pfileid = (int*) param;
   fileid = *pfileid;
   free(pfileid);
 
   sock = exp1_tcp_connect(g_hostname, 10046);
   if(sock < 0){
     exp1_session_error();
     pthread_exit(NULL);
   }

char postdata[128];
int postlen = sprintf(postdata, "file=%03d.jpg", fileid);

sprintf(command,
  "POST /get_image HTTP/1.0\r\n"
  "Host: %s\r\n"
  "Content-Type: application/x-www-form-urlencoded\r\n"
  "Content-Length: %d\r\n"
  "\r\n"
  "%s\r\n",
  g_hostname,
  postlen,
  postdata
);


   ret = send(sock, command, strlen(command), 0);
   if(ret < 0){
     exp1_session_error();
     pthread_exit(NULL);
   }
 
   total = 0;
   ret = recv(sock, buf, 2048, 0);
 
   char res_buf[2048];
   strcpy(res_buf, buf);
   char* res_code = strtok(res_buf, " ");
   res_code = strtok(NULL, " ");
 
   while(ret > 0){
     total += ret;
     ret = recv(sock, buf, 2048, 0);
   }
   fprintf(stderr, "POST /get_image %s/%03d.jpg HTTP/1.0", base_path, fileid);
   fprintf(stderr, " -> Response Code: %s: Recieved Size: %d bytes\n", res_code, total);
 
   pthread_exit(NULL);
 }

 void exp1_session_error() {
   pthread_mutex_lock(&g_mutex);
   g_error_count++;
   pthread_mutex_unlock(&g_mutex);
 
   return;
 }
