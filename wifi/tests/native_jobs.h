static void test_job_word(unsigned at,unsigned v){job_upload[at]=v;job_upload[at+1]=v>>8;job_upload[at+2]=v>>16;job_upload[at+3]=v>>24;}
static void test_job_elf(void){memset(job_upload,0,sizeof(job_upload));memcpy(job_upload,"\177ELF\1\1\1",7);job_upload[16]=2;job_upload[18]=8;test_job_word(24,(unsigned)job_arena);test_job_word(28,52);job_upload[42]=32;job_upload[44]=1;
 test_job_word(52,1);test_job_word(56,128);test_job_word(60,(unsigned)job_arena);test_job_word(68,12);test_job_word(72,12);test_job_word(76,5);test_job_word(128,0x2402002a);test_job_word(132,0x03e00008);test_job_word(136,0);job_source_mode=0;job_size=job_received=140;job_state=1;job_done=0;job_output_length=0;}
static int test_native_jobs(void){char out[9000];unsigned ticks;int code;
 test_job_elf();test_job_word(72,JOB_ARENA_SIZE+1);if(!job_load())return 1;
 test_job_elf();test_job_word(56,JOB_UPLOAD_SIZE);if(!job_load())return 2;
 test_job_elf();test_job_word(24,(unsigned)job_arena+1000);if(!job_load())return 3;
 test_job_elf();code=job_http("POST","/exec/run","\r\nX-PS2-Exec: 1\r\n",0,0,millis(),out,sizeof(out));if(code!=200)return 4;
 DelayThread(100000);job_tick(millis());if(job_state!=3||job_result!=42)return 5;
 test_job_elf();test_job_word(128,0x1000ffff);test_job_word(132,0);ticks=runtime.pad_ticks;
 if(job_http("POST","/exec/run","\r\nX-PS2-Exec: 1\r\n",0,0,millis(),out,sizeof(out))!=200)return 6;
 DelayThread(300000);job_tick(job_started+2001);if(job_state!=4||runtime.pad_ticks-ticks<5){printf("JOB watchdog state=%u ticks=%u tid=%d done=%u\n",job_state,(unsigned)(runtime.pad_ticks-ticks),job_thread_id,job_done);return 7;}
 test_job_elf();if(job_http("POST","/exec/run","\r\n",0,0,millis(),out,sizeof(out))!=403)return 8;
 if(!job_auth("POST /exec/run HTTP/1.1\r\nx-ps2-exec: 1\r\n\r\n")||job_auth("POST /exec/run HTTP/1.1\r\n\r\nx-ps2-exec: 1\r\n"))return 9;
 printf("NATIVE_JOB_TEST PASS actual-MIPS-return=42 invalid-segments-entry-rejected timeout-stops-worker gamepad-alive\n");return 0;
}
static int test_native_relay_recovery(void){uint32_t until;unsigned ticks=runtime.pad_ticks;control_ready=0;
 until=millis()+3000;while(SifCheckStatRpc(&robot_rpc)&&(int32_t)(millis()-until)<0)DelayThread(1000);robot_pending=0;
 if(SifCheckStatRpc(&robot_rpc)||!robot_rpc.server)return 1;
 memset(&robot_packet,0,sizeof(robot_packet));
 if(SifCallRpc(&robot_rpc,SEDA_RELAY_RPC_RECOVER,SIF_RPC_M_NOWAIT,&robot_packet,sizeof(robot_packet),&robot_packet,sizeof(robot_packet),0,0)<0)return 2;
 until=millis()+4000;while(SifCheckStatRpc(&robot_rpc)&&(int32_t)(millis()-until)<0)DelayThread(1000);
 if(SifCheckStatRpc(&robot_rpc)||robot_packet.result||robot_packet.status!=SEDA_RELAY_STATUS_DISCONNECTED||runtime.pad_ticks-ticks<30){printf("RELAY recovery status=%d result=%d ticks=%u busy=%d\n",robot_packet.status,robot_packet.result,(unsigned)(runtime.pad_ticks-ticks),SifCheckStatRpc(&robot_rpc));return 3;}
 control_ready=1;printf("NATIVE_RELAY_RECOVERY_TEST PASS actual-IOP-LDD-rebind pad-alive no-RF-device no-relay-device\n");return 0;
}
static int test_native_offline_recovery(void){unsigned state=app.state,linked=app.linked,actions=app.actions,ticket=app.ticket,ready=physical_radio_ready;uint32_t until,ticks;
 offline_usb_at=0;offline_usb_wanted=0;app.linked=0;offline_tick(100);offline_tick(30099);if(offline_usb_wanted)return 1;
 offline_tick(30100);if(!offline_usb_wanted)return 2;
 app.linked=1;offline_tick(40000);if(offline_usb_wanted||offline_usb_at!=70000)return 3;
 app.linked=0;offline_tick(69999);if(offline_usb_wanted)return 4;offline_tick(70000);if(!offline_usb_wanted)return 5;
 physical_radio_ready=1;app.state=WIFI_DHCP;offline_probe_done(1,70000);if(app.state!=WIFI_DHCP)return 6;
 app.state=WIFI_LIST;offline_probe_done(0,70000);if(app.state!=WIFI_LIST)return 7;
 offline_probe_done(1,70000);if(app.state!=WIFI_SCANNING||!(app.actions&WIFI_DO_SCAN))return 8;
 app.state=state;app.linked=linked;app.actions=actions;app.ticket=ticket;physical_radio_ready=ready;offline_usb_wanted=0;offline_usb_at=millis()+30000;
 until=millis()+4000;while(SifCheckStatRpc(&client)&&(int32_t)(millis()-until)<0)DelayThread(1000);pending=0;
 if(request(RTL_RPC_USB_CHECK,millis()))return 9;
 until=millis()+4000;while(SifCheckStatRpc(&client)&&(int32_t)(millis()-until)<0)DelayThread(1000);
 if(SifCheckStatRpc(&client)||response.inventory.present)return 10;pending=0;
 ticks=runtime.pad_ticks;DelayThread(50000);if(runtime.pad_ticks-ticks<3)return 11;
 printf("NATIVE_OFFLINE_RECOVERY_TEST PASS 30s-boundary online-suppressed no-adapter-no-join DHCP-not-interrupted actual-IOP-cached-USB-scan pad-alive\n");return 0;
}
static int test_native_sources(void){static const struct{const char*source;unsigned lang;int answer;const char*output;}cases[]={
 {"int main(){int a=6;print(\"PS2 C++: \");print(a*7);return a*7;}",0,42,"PS2 C++: 42"},
 {"int main(){int i=0;int sum=0;while(i<7){sum=sum+i;i=i+1;}if(sum==21)return sum;else return -1;}",0,21,""},
 {"int main(){int a=5;return (a<<3)+((-9)/3)+5;}",0,42,""},
 {"int main(){if(false&&print(\"BAD\"))return 1;if(true||print(\"BAD\"))return 42;return 0;}",0,42,""},
 {"li $t0,0\nli $t1,7\nloop:\naddiu $t0,$t0,1\nbne $t0,$t1,loop\nmove $v0,$t0\nret\n",1,7,""},
 {"text \"MIPS: \"\nli $v0,42\nprint $v0\nli $v0,42\nret\n",1,42,"MIPS: 42"},
 {"li $a0,0\nmove $a1,$s1\nli $v1,48\n.word 0x0000000c\nlw $v0,0($s1)\nret\n",1,1,""}};unsigned i;char out[9000];
 for(i=0;i<sizeof(cases)/sizeof(cases[0]);i++){uint32_t until;const char*t=cases[i].lang?"/exec/source?lang=asm":"/exec/source?lang=cpp";int code=job_http("POST",t,"\r\nX-PS2-Exec: 1\r\n",(const uint8_t*)cases[i].source,strlen(cases[i].source),millis(),out,sizeof(out));if(code!=200)return 10+i;
  until=millis()+1500;while(!job_done&&(int32_t)(millis()-until)<0)DelayThread(1000);job_tick(millis());
  if(job_state!=3||job_result!=cases[i].answer||strcmp(job_output,cases[i].output)){printf("SOURCE case=%u state=%u result=%d output=%s\n",i,job_state,job_result,job_output);return 20+i;}}
 {const char*bad="class X {};";job_http("POST","/exec/source?lang=cpp","\r\nX-PS2-Exec: 1\r\n",(const uint8_t*)bad,strlen(bad),millis(),out,sizeof(out));DelayThread(100000);job_tick(millis());if(job_state!=7)return 30;}
 printf("NATIVE_SOURCE_COMPILER_TEST PASS PS2-local-C++-subset-and-assembler cases=7 arithmetic-loops-labels-output-short-circuit-errors-kernel-thread-query\n");return 0;
}
