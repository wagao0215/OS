/************************************************************************

 This code forms the base of the operating system you will
 build.  It has only the barest rudiments of what you will
 eventually construct; yet it contains the interfaces that
 allow test.c and z502.c to be successfully built together.

 Revision History:
 1.0 August 1990
 1.1 December 1990: Portability attempted.
 1.3 July     1992: More Portability enhancements.
 Add call to SampleCode.
 1.4 December 1992: Limit (temporarily) printout in
 interrupt handler.  More portability.
 2.0 January  2000: A number of small changes.
 2.1 May      2001: Bug fixes and clear STAT_VECTOR
 2.2 July     2002: Make code appropriate for undergrads.
 Default program start is in test0.
 3.0 August   2004: Modified to support memory mapped IO
 3.1 August   2004: hardware interrupt runs on separate thread
 3.11 August  2004: Support for OS level locking
 4.0  July    2013: Major portions rewritten to support multiple threads
 4.20 Jan     2015: Thread safe code - prepare for multiprocessors
 ************************************************************************/

#include             "global.h"
#include             "syscalls.h"
#include             "protos.h"
//#include             "process.h"
#include             "string.h"
#include             <stdlib.h>
#include             <ctype.h>
#include             <stdio.h>
#include             <ctype.h>
#include             "stdio.h"
#include             "stdlib.h"
#include              "math.h"
//#include             <malloc.h>
//#include             <z502.h>
//#include             <syscalls.h>


#define    STARTSECTOR                   10

//  Allows the OS and the hardware to agree on where faults occur
extern void *TO_VECTOR[];

char *call_names[]={ "mem_read ", "mem_write", "read_mod ", "get_time ",
		"sleep    ", "get_pid  ", "create   ", "term_proc", "suspend  ",
		"resume   ", "ch_prior ", "send     ", "receive  ", "PhyDskRd ",
		"PhyDskWrt", "def_sh_ar", "Format   ", "CheckDisk", "Open_Dir ",
        "OpenFile ", "Crea_Dir ", "Crea_File", "ReadFile ", "WriteFile",
		"CloseFile", "DirContnt", "Del_Dir  ", "Del_File "};


//PCB structure

typedef struct  {
		long       ProcessID;
		char       Process_state;
		char      * Process_Name;
		long       Context;
		long       Priority;
		long       *Pagetable;
        int        shadowpage[NUMBER_VIRTUAL_PAGES];
	} PROCESS_CONTROL_BLOCK;// the data structure of PCB

int PROCESSid;

PROCESS_CONTROL_BLOCK pcb;// this is a PCB data structure,which can be used in all function.

PROCESS_CONTROL_BLOCK *pcbAddress;

PROCESS_CONTROL_BLOCK *pcblist[15];// its is a list to record all the process we create .

typedef struct
{
    INT32 valid_number[NUMBER_PHYSICAL_PAGES];
    long  ProcessID[NUMBER_PHYSICAL_PAGES];
    long  logic_address[NUMBER_PHYSICAL_PAGES];
    
}Frame_table;

Frame_table   *FT;

void INITframetable()
{
    short  frameindex;
    FT = (Frame_table *) calloc(1, sizeof(Frame_table));
    
    for (frameindex=0;frameindex<NUMBER_PHYSICAL_PAGES;frameindex++){
        FT->valid_number[frameindex] = 0;
        FT->ProcessID[frameindex] = 0;
        FT->logic_address[frameindex] = 0;
    }
    
}


void OSCreateProcess(long*testname) {//testname is the test1 or test2  you want to start.
    
	void *PageTable = (void *) calloc(2, NUMBER_VIRTUAL_PAGES);
	MEMORY_MAPPED_IO mmio;
	PROCESSid=2;//this is a processID adder, which means once it increase by 1 after it assign to a new process.
    long index;


	mmio.Mode = Z502InitializeContext;
	mmio.Field1 = 0;
	mmio.Field2 = (long) testname;
	mmio.Field3 = (long) PageTable;
	MEM_WRITE(Z502Context, &mmio);

	pcb.ProcessID=PROCESSid;
	PROCESSid=PROCESSid+1;
	pcb.Context=mmio.Field1;
	pcb.Process_Name = (char*)testname;
    pcb.Pagetable = PageTable;
    for (index=0; index<=NUMBER_VIRTUAL_PAGES;index++){
        pcb.shadowpage[index] = -1;
    }
    INITframetable();
	mmio.Mode = Z502StartContext;
	mmio.Field2 = START_NEW_CONTEXT_AND_SUSPEND;
	MEM_WRITE(Z502Context, &mmio);     // Start up the context

}


/************************************************************************
 ready queue
 it includes the queue functions like enqueue and dequeue and get the length of
 the queue. each time a process created , it will enqueue in this ready queue.
 each node date is the pointer of the PCB
 ************************************************************************/
typedef struct qnode
	{
	    PROCESS_CONTROL_BLOCK *data;
		struct qnode *next;

	}Qnode;

typedef struct
	{
		Qnode *front;
		Qnode *rear;
	}ReadyQueue;

ReadyQueue  Rqueue;

ReadyQueue  timerqueue;

void initQueue(ReadyQueue *i)
{

	i->front=i->rear=(Qnode*)malloc(sizeof(Qnode));
	if (NULL==i->front){
		exit(0);}
	i->front->next=NULL;
}


void enQueue(ReadyQueue *q,PROCESS_CONTROL_BLOCK *e)// enqueue of the process pointer.
{
	PROCESS_CONTROL_BLOCK *ad;
	ad=(PROCESS_CONTROL_BLOCK *)malloc(sizeof(PROCESS_CONTROL_BLOCK ));//
	*ad=*e;

	Qnode *s;
	s=(Qnode*)malloc(sizeof(Qnode));
	s->data=ad;
	s->next=NULL;

	if(q->rear==NULL&&q->front==NULL)
	{
		q->front=s;
		q->rear=s;
	}
	else
	{
		q->rear->next=s;
		q->rear=s;
	}
}

int deQueue(ReadyQueue *q,PROCESS_CONTROL_BLOCK *e)
{
	Qnode *t;
	if(q->rear==NULL){
		return 0;}
	if(q->front==q->rear)
	{
		t=q->front;
		q->front=NULL;
		q->rear=NULL;
	}
	else
	{
		t=q->front;
		q->front=q->front->next;
	}
	e=t->data;
	free(t);
	return 1;
}

int Lenth(ReadyQueue *q)
{
	int n=0;
	Qnode *p;
	p=q->front;
	while(p!=NULL)
	{
		p=p->next;
		n++;
	}
	return n;

}

void addlist(PROCESS_CONTROL_BLOCK *e)
{
	int i=0;
	for (i=0;i<=14;i++)
		if(pcblist[i]==NULL){
			pcblist[i]=e;
			break;
		}
}

int Dispatcher(ReadyQueue i)
{

	Qnode *temp = i.front;

	if(temp!=NULL)

		return 1;

	else

		return 0;
}





/************************************************************************
 INTERRUPT_HANDLER
 When the Z502 gets a hardware interrupt, it transfers control to
 this routine in the OS.
 ************************************************************************/
void InterruptHandler(void) {
	INT32 DeviceID;
	INT32 Status;
	MEMORY_MAPPED_IO mmio;       // Enables communication with hardware

	static BOOL remove_this_in_your_code = TRUE; /** TEMP **/
	static INT32 how_many_interrupt_entries = 0; /** TEMP **/

	// Get cause of interrupt
	mmio.Mode = Z502GetInterruptInfo;
	mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
	MEM_READ(Z502InterruptDevice, &mmio);
	DeviceID = mmio.Field1;
	Status = mmio.Field2;

	deQueue(&timerqueue,pcbAddress);
	/** REMOVE THE NEXT SIX LINES **/
	how_many_interrupt_entries++; /** TEMP **/
	if (remove_this_in_your_code && (how_many_interrupt_entries < 10)) {
		printf("Interrupt_handler: Found device ID %d with status %d\n",
				(int) mmio.Field1, (int) mmio.Field2);
	}

	// Clear out this device - we're done with it
	mmio.Mode = Z502ClearInterruptStatus;
	mmio.Field1 = DeviceID;
	mmio.Field2 = mmio.Field3 = 0;
	MEM_WRITE(Z502InterruptDevice, &mmio);
}           // End of InterruptHandler

/************************************************************************
 FAULT_HANDLER
 The beginning of the OS502.  Used to receive hardware faults.
 ************************************************************************/

long  alternum=0;// the frame number of frist in first out algorithm , after we assign a frame this will increase by 1 until to 64, and reset to 0 again

int y[2000]={0};// swap area of the disk
int x;
long  Sector;

 

void FaultHandler(void) {
	INT32 DeviceID;
	INT32 Status;
	INT32 ptb_bits;
	UINT16 *pg;
    long ErrorReturned;
	MEMORY_MAPPED_IO mmio;       // Enables communication with hardware
    short  count;
    INT32 Validbit;
    BOOL  AllFT;
    long PhysicalFrameNumber;
    char  Data[PGSIZE];         // data swap out or swap in
    long  DiskID=1;
    long  Validad;
    long  shadowpagenum;       // whether data in disk, it will be sector number in shadowpage ,if it not disk ,this is -1.
    MP_INPUT_DATA MPData1;
    long  j;
    
    
	// Get cause of interrupt
	mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
	mmio.Mode = Z502GetInterruptInfo;
	MEM_READ(Z502InterruptDevice, &mmio);
	DeviceID = mmio.Field1;
	Status = mmio.Field2;


	printf("Fault_handler: Found vector type %d with value %d\n", DeviceID,
			Status);
    
    if( Status > 1023){
        
        TERMINATE_PROCESS(-1, &ErrorReturned);
    }  // terminate process if we get a address number biger than 1023, the number of logical number

		mmio.Mode = Z502GetPageTable;
		mmio.Field1 = mmio.Field2=mmio.Field3=mmio.Field4=0;
		MEM_READ(Z502Context, &mmio);
		pg = (UINT16 *) mmio.Field1;// get current page table of the process
        
        shadowpagenum=pcb.shadowpage[Status]; // get sector number if the data in disk
        
// if data is not in disk ,which means the number is -1 in shadow page ,then we will see the frame availabe to this address. we first go over the frame table to see whick frame is free ,and after all the frames are allocated to other process ,we will do the swap out based on the first in first out algorithm.
    
        if(shadowpagenum == -1){
            AllFT = FALSE;
            for (count=0;count<NUMBER_PHYSICAL_PAGES;count++){
            Validbit = FT->valid_number[count];
            if(Validbit==0){
                ptb_bits = PTBL_VALID_BIT;
                pg[Status]|= ptb_bits;
                ptb_bits = PTBL_PHYS_PG_NO & count ;
                pg[Status]|= ptb_bits;
                FT->valid_number[count]=1;
                FT->ProcessID[count]=pcb.ProcessID;
                FT->logic_address[count] = Status;
                AllFT = TRUE;
                break;}
            else
                continue ;}
 // if all 64 frames are set validbit 1 , then we start to swap out data to disk.
            
           if (AllFT == FALSE){
            PhysicalFrameNumber=alternum;
            Z502ReadPhysicalMemory(PhysicalFrameNumber,Data);  // get the data from frame
            for (count=0;count<2000;count++){
                
                if (y[count]==0){
                    Sector=STARTSECTOR+count;
                    x=1;
                    y[count]= x;
                    mmio.Mode = Z502DiskWrite;// write the data to the disk.
                    mmio.Field1 = DiskID;
                    mmio.Field2 = (long)Sector;
                    mmio.Field3 = (long)Data;
                    MEM_WRITE(Z502Disk, &mmio);
                    
                    mmio.Mode = Z502Action;
                    mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
                    MEM_WRITE(Z502Idle, &mmio);
                    
                    mmio.Field2 = DEVICE_IN_USE;
                    while (mmio.Field2 != DEVICE_FREE) {
                        mmio.Mode = Z502Status;
                        mmio.Field1 = DiskID;
                        mmio.Field2 = mmio.Field3 = 0;
                        MEM_READ(Z502Disk, &mmio);
                    }

                    break;}
                else
                    continue;
                    
            }
        
            Validad = FT->logic_address[alternum];// update the shadow page , record the sector number in the shadow page,
            pcb.shadowpage[Validad] = Sector;
               
            ptb_bits = PTBL_VALID_BIT;// updata the valid bit =0 in page table of swaped out adress.
            pg[Validad]^= ptb_bits;
            ptb_bits = ~PTBL_PHYS_PG_NO;
            pg[Validad]&=ptb_bits;
            
            ptb_bits = PTBL_VALID_BIT;// set valid bit =1 of the new address in pagetable
            pg[Status]|= ptb_bits;
            ptb_bits = PTBL_PHYS_PG_NO & alternum ;// set physic bit = frame number of the new address in pagetable
            pg[Status]|= ptb_bits;
        
            FT->valid_number[alternum] = 1;// update the frame table ,1 means that the frame number is assigned to a logical number.
            FT->ProcessID[alternum]=pcb.ProcessID;
            FT->logic_address[alternum] = Status;
            alternum=alternum+1;
               
            //  we swap out next frame next time and until we 64 times we will get down to the buttom of frame table , then we will start from top
            if (alternum==64){
                
                alternum=0;
            }}}
        
        
        else{                                         // if sector in shadow page ,then we swap out data first then we swap in from the sector we get .
            PhysicalFrameNumber = alternum;
            Z502ReadPhysicalMemory(PhysicalFrameNumber,Data);
           
            for (count=0;count<2000;count++){
                
                if (y[count]==0){
                    Sector=STARTSECTOR+count;
                    x=1;
                    y[count]= x;
                    mmio.Mode = Z502DiskWrite;
                    mmio.Field1 = DiskID;
                    mmio.Field2 = (long)Sector;
                    mmio.Field3 = (long)Data;
                    MEM_WRITE(Z502Disk, &mmio);
                    
                    mmio.Mode = Z502Action;
                    mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
                    MEM_WRITE(Z502Idle, &mmio);
                    
                    mmio.Field2 = DEVICE_IN_USE;
                    while (mmio.Field2 != DEVICE_FREE) {
                        mmio.Mode = Z502Status;
                        mmio.Field1 = DiskID;
                        mmio.Field2 = mmio.Field3 = 0;
                        MEM_READ(Z502Disk, &mmio);
                    }
                    break;}
                else
                    continue;
            }
            
            
            
            Validad = FT->logic_address[alternum];
            
            
            pcb.shadowpage[Validad] = Sector;
            ptb_bits = PTBL_VALID_BIT;
            pg[Validad]^= ptb_bits;
            ptb_bits = ~PTBL_PHYS_PG_NO;
            pg[Validad]&=ptb_bits;
            
            
            mmio.Mode = Z502DiskRead;
            mmio.Field1 = DiskID;
            mmio.Field2 = (long)shadowpagenum;
            mmio.Field3 = (long) Data;
            MEM_WRITE(Z502Disk, &mmio);
            
            mmio.Mode = Z502Action;
            mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
            MEM_WRITE(Z502Idle, &mmio);
            
            mmio.Field2 = DEVICE_IN_USE;
            while (mmio.Field2 != DEVICE_FREE) {
                mmio.Mode = Z502Status;
                mmio.Field1 = DiskID;
                mmio.Field2 = mmio.Field3 = 0;
                MEM_READ(Z502Disk, &mmio);
            }
            
            PhysicalFrameNumber = alternum;
            Z502WritePhysicalMemory(PhysicalFrameNumber,Data);// read data from disk and write into the frame.
            

            
//            y[shadowpagenum-STARTSECTOR]=0;
    
            FT->valid_number[alternum]=1;
            FT->ProcessID[alternum]=pcb.ProcessID;
            FT->logic_address[alternum] = Status;
            
            ptb_bits = PTBL_VALID_BIT;             // give the frame to the logical address which set the fault handler.
            pg[Status]|= ptb_bits;
            ptb_bits = PTBL_PHYS_PG_NO & alternum ;
            pg[Status]|= ptb_bits;
            
            Validad = FT->logic_address[alternum];
            pcb.shadowpage[Validad] = -1;
            
            alternum=alternum+1;
            if (alternum==64){
                alternum=0;}
        }
    
        mmio.Mode = Z502ClearInterruptStatus;
        mmio.Field1 = DeviceID;
        MEM_WRITE(Z502InterruptDevice, &mmio);
    // clear the interrupt.
   
    
    memset(&MPData1, 0, sizeof(MP_INPUT_DATA));
    
    for (j = 0; j < NUMBER_PHYSICAL_PAGES ; j = j + 2) {
        
        if (FT->valid_number[j]==1){
          MPData1.frames[j].InUse = TRUE;
        }
        else {
          MPData1.frames[j].InUse =  FALSE;
        }
        MPData1.frames[j].Pid = FT->ProcessID[j];
        MPData1.frames[j].LogicalPage = FT->logic_address[j];
        if (((pg[FT->logic_address[j]]&PTBL_VALID_BIT)==1)&&((pg[FT->logic_address[j]]&PTBL_MODIFIED_BIT)==1)&&((pg[FT->logic_address[j]]&PTBL_REFERENCED_BIT)==1)){
            
        MPData1.frames[j].State= FRAME_VALID+FRAME_MODIFIED+FRAME_REFERENCED;}
        
        if (((pg[FT->logic_address[j]]&PTBL_VALID_BIT)==1)&&((pg[FT->logic_address[j]]&PTBL_MODIFIED_BIT)==1)){
            MPData1.frames[j].State= FRAME_VALID+FRAME_MODIFIED;
        }
        
        if (((pg[FT->logic_address[j]]&PTBL_VALID_BIT)==1)){
            MPData1.frames[j].State= FRAME_VALID;
        }
        
        if (((pg[FT->logic_address[j]]&PTBL_VALID_BIT)==1)){
            MPData1.frames[j].State= 0;
        }

    }
    
    MPPrintLine(&MPData1);  // print the memory information in detail .
    
    
    
} // End of FaultHandler

/************************************************************************
 SVC
 The beginning of the OS502.  Used to receive software interrupts.
 All system calls come to this point in the code and are to be
 handled by the student written code here.
 The variable do_print is designed to print out the data for the
 incoming calls, but does so only for the first ten calls.  This
 allows the user to see what's happening, but doesn't overwhelm
 with the amount of data.
 ************************************************************************/


void svc(SYSTEM_CALL_DATA *SystemCallData) {
	short call_type;
	static short do_print = 10;
	short i;
	INT32 Status;
	MEMORY_MAPPED_IO  mmio;

    PROCESS_CONTROL_BLOCK temppcb;// only used in this function
    long j;
    char *p;
    void *PageTable = (void *) calloc(2, NUMBER_VIRTUAL_PAGES);

	call_type = (short) SystemCallData->SystemCallNumber;
	if (do_print > 0) {
		printf("SVC handler: %s\n", call_names[call_type]);
		for (i = 0; i < SystemCallData->NumberOfArguments - 1; i++) {
			//Value = (long)*SystemCallData->Argument[i];
			printf("Arg %d: Contents = (Decimal) %8ld,  (Hex) %8lX\n", i,
					(unsigned long) SystemCallData->Argument[i],
					(unsigned long) SystemCallData->Argument[i]);
		}
		do_print--;
	}
	switch (call_type){

	    case SYSNUM_GET_TIME_OF_DAY:// for test0 to get of the time

	    mmio.Mode = Z502ReturnValue;
	    mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
	    MEM_READ(Z502Clock, &mmio);
	    *(long*)SystemCallData->Argument[0]=mmio.Field1;

	    break;



        case SYSNUM_SLEEP:

        pcbAddress=&pcb;
        enQueue(&timerqueue,pcbAddress);
        printf("%d\n",timerqueue.front->data->ProcessID);

        mmio.Mode = Z502Start;
        mmio.Field1 = (long)SystemCallData->Argument[0];
        mmio.Field2 = mmio.Field3 = 0;
        MEM_WRITE(Z502Timer, &mmio);



        mmio.Mode = Z502Status;
        mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
        MEM_READ(Z502Timer, &mmio);
        Status = mmio.Field1;

        mmio.Mode = Z502Action;
        mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
        MEM_WRITE(Z502Idle, &mmio);

        int i=Dispatcher(Rqueue);

        if(i){

        	mmio.Mode = Z502StartContext;
        	mmio.Field1=Rqueue.front->data->Context;
        	mmio.Field2 = START_NEW_CONTEXT_AND_SUSPEND;
        	MEM_WRITE(Z502Context, &mmio);
        };


        break;





        case  SYSNUM_GET_PROCESS_ID:// to get the process ID

         p=(char*)SystemCallData->Argument[0];
         printf("%s\n", p);

         if(strcmp(p,"")==0){
         // when process name is nothing ,then give the current process ID
        	 *(long*)SystemCallData->Argument[1]=pcb.ProcessID;
        	 *SystemCallData->Argument[2]=ERR_SUCCESS;
        	 break;
         }

         else{

         Qnode *temp1 = Rqueue.front;

         while (temp1!=NULL)
         {
        	 //printf("%s\n", temp1->data->Process_Name);

        	if(strcmp(temp1->data->Process_Name,p)==0)// TO check which process name in ready queue ,and return the correspondance process ID
        	{

        		*SystemCallData->Argument[1]=temp1->data->ProcessID;
        		*SystemCallData->Argument[2]=ERR_SUCCESS;
        		break;// after we get the processID, We break .
        	    		        	}
        	else{

        		*SystemCallData->Argument[2]=-1;
        		break;
        	}
        	temp1 = temp1 -> next;
        	    		        }

        }

		break;




	    case SYSNUM_TERMINATE_PROCESS:
        // according to the parameters we firstly  make sure which process to terminate by go through the ready queue.
	    // if it is the process ID is -1 we will terminate the current process , -2 we will terminate all and others we
	    // terminate the one that is the same to the request.

	    	j=(long)SystemCallData->Argument[0];


	    	if (j==-1){
	    		mmio.Mode = Z502Action;
	    		mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
	    		MEM_WRITE(Z502Halt, &mmio);
	    	}
	    	else if (j==-2){
	    		mmio.Mode = Z502Action;
	    	    mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
	    		MEM_WRITE(Z502Halt, &mmio);
	    	}
	    	else{
	    		Qnode *temp = Rqueue.front;
	    	    while(temp!=NULL){

	    		 if(temp->data->ProcessID==j){

	    		    //mmio.Mode = Z502Action;
	    		    //mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
	    		    //MEM_WRITE(Z502Halt, &mmio);
	    			 deQueue(&Rqueue,pcbAddress);
	    			 *SystemCallData->Argument[1]=ERR_SUCCESS;
	    			 break;
	    		        	}
	    		    temp = temp -> next;
	    		        }

	    	}

        break;




	    case SYSNUM_PHYSICAL_DISK_WRITE://check status of the disk first , the same to sample.c

	    mmio.Mode = Z502Status;
	    mmio.Field1 = (long)SystemCallData->Argument[0];
	    mmio.Field2 = mmio.Field3 = 0;
	    MEM_READ(Z502Disk, &mmio);
	    if (mmio.Field2 == DEVICE_FREE)    // Disk hasn't been used - should be free
	    	printf("Disk Test 1: Got expected result for Disk Status\n");
	    else
	    	printf("Disk Test 1: Got erroneous result for Disk Status - Device not free.\n");

	    mmio.Mode = Z502DiskWrite;
	    mmio.Field1 = (long)SystemCallData->Argument[0];
	    mmio.Field2 = (long)SystemCallData->Argument[1];
	    mmio.Field3 = (long)SystemCallData->Argument[2];
	    MEM_WRITE(Z502Disk, &mmio);

	    mmio.Mode = Z502Status;
	    mmio.Field1 = (long)SystemCallData->Argument[0];
	    mmio.Field2 = mmio.Field3 = 0;
	    MEM_READ(Z502Disk, &mmio);
	    if (mmio.Field2 == DEVICE_IN_USE)        // Disk should report being used
	    	printf("Disk Test 2: Got expected result for Disk Status\n");
	    else
	    	printf("Disk Test 2: Got erroneous result for Disk Status\n");

	    mmio.Mode = Z502Action;
	    mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
	    MEM_WRITE(Z502Idle, &mmio);

	    mmio.Mode = Z502Status;
	    mmio.Field1 =(long)SystemCallData->Argument[0] ;
	    mmio.Field2 = mmio.Field3 = 0;
	    MEM_READ(Z502Disk, &mmio);
	    if (mmio.Field2 == DEVICE_FREE)        // Disk should be free
	    	printf("Disk Test 3: Got expected result for Disk Status\n");
	    else
	    	printf("Disk Test 3: Got erroneous result for Disk Status - Device not free.\n");

	    break;

	    case SYSNUM_PHYSICAL_DISK_READ://the same to sample.c

	   	mmio.Mode = Z502DiskRead;
	   	mmio.Field1 = (long)SystemCallData->Argument[0];
	   	mmio.Field2 = (long)SystemCallData->Argument[1];
	   	mmio.Field3 = (long)SystemCallData->Argument[2];
	   	MEM_WRITE(Z502Disk, &mmio);


	   	mmio.Mode = Z502Action;
	    mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
	   	MEM_WRITE(Z502Idle, &mmio);

	   	mmio.Mode = Z502Status;
	   	mmio.Field1 =(long)SystemCallData->Argument[0] ;
	   	mmio.Field2 = mmio.Field3 = 0;
	   	MEM_READ(Z502Disk, &mmio);
	   	if (mmio.Field2 == DEVICE_FREE)        // Disk should be free
	   	   printf("Disk Test 3: Got expected result for Disk Status\n");
	   	else
	   	   printf("Disk Test 3: Got erroneous result for Disk Status - Device not free.\n");

	   	break;




	    case SYSNUM_CREATE_PROCESS:// to create a new process and put it in the ready queue


	    mmio.Mode = Z502InitializeContext;
	    mmio.Field1 = 0;
	    mmio.Field2 = (long)SystemCallData->Argument[1] ;
	    mmio.Field3 = (long) PageTable;
	    MEM_WRITE(Z502Context, &mmio);

	    temppcb.Context=mmio.Field1;// record the context of the process
	    temppcb.Pagetable=PageTable;


	    temppcb.ProcessID=PROCESSid;// give the ID of process
    	PROCESSid=PROCESSid+1;
	    *(long*)SystemCallData->Argument[3]=temppcb.ProcessID;
	    temppcb.Priority=(long)SystemCallData->Argument[2];

	    char* process_name = (char *)malloc(sizeof(char)* (strlen((char*)SystemCallData->Argument[0]) + 1));
	    strcpy(process_name, (char*)SystemCallData->Argument[0]);
	    temppcb.Process_Name = process_name;
	    // this is to copy the name of the process and give it to processID

	    if (temppcb.Priority<0)// to check the illegal or legal of the priority of the process.
	    *(long*)SystemCallData->Argument[4]=-1;
	    else
	    *(long*)SystemCallData->Argument[4]=ERR_SUCCESS;


	    Qnode *temp = Rqueue.front;// to see whether the ready queue has a same name process
        while(temp!=NULL){


        	if(strcmp(temp->data->Process_Name, temppcb.Process_Name) == 0){
        		*SystemCallData->Argument[4]=-1;
        		break;
        	}
        	else {
        		*SystemCallData->Argument[4]=ERR_SUCCESS;
        	}
        	temp = temp -> next;
        }
        pcbAddress=&temppcb;

	    if (*SystemCallData->Argument[4] == ERR_SUCCESS) {///this is to enqueue the correct process.
	    	enQueue(&Rqueue,pcbAddress);
	    	addlist(pcbAddress);
	    	}

	    int m=Lenth(&Rqueue);//// to check how many process in the ready queue
	    if (m>=15){
	    	*SystemCallData->Argument[4]=-1;
	    }


		//mmio.Mode = Z502StartContext;
		//mmio.Field2 = START_NEW_CONTEXT_AND_SUSPEND;
		//MEM_WRITE(Z502Context, &mmio);

	    break;


	    default:
	    	printf("ERROE!call type not recognized !\n");
	    	printf("call type is-%i\n",call_type);


	}
}
// End of svc

/************************************************************************
 osInit
 This is the first routine called after the simulation begins.  This
 is equivalent to boot code.  All the initial OS components can be
 defined and initialized here.
 ************************************************************************/

void osInit(int argc, char *argv[]) {
	void *PageTable = (void *) calloc(2, NUMBER_VIRTUAL_PAGES);
	INT32 i;
	MEMORY_MAPPED_IO mmio;
	// Demonstrates how calling arguments are passed thru to here

	printf("Program called with %d arguments:", argc);
	for (i = 0; i < argc; i++)
		printf(" %s", argv[i]);
	printf("\n");
	printf("Calling with argument 'sample' executes the sample program.\n");

	// Here we check if a second argument is present on the command line.
	// If so, run in multiprocessor mode
	if (argc > 2) {
		if ( strcmp( argv[2], "M") || strcmp( argv[2], "m")) {
		printf("Simulation is running as a MultProcessor\n\n");
		mmio.Mode = Z502SetProcessorNumber;
		mmio.Field1 = MAX_NUMBER_OF_PROCESSORS;
		mmio.Field2 = (long) 0;
		mmio.Field3 = (long) 0;
		mmio.Field4 = (long) 0;
		MEM_WRITE(Z502Processor, &mmio);   // Set the number of processors
		}
	} else {
		printf("Simulation is running as a UniProcessor\n");
		printf(
				"Add an 'M' to the command line to invoke multiprocessor operation.\n\n");
	}

	//          Setup so handlers will come to code in base.c

	TO_VECTOR[TO_VECTOR_INT_HANDLER_ADDR ] = (void *) InterruptHandler;
	TO_VECTOR[TO_VECTOR_FAULT_HANDLER_ADDR ] = (void *) FaultHandler;
	TO_VECTOR[TO_VECTOR_TRAP_HANDLER_ADDR ] = (void *) svc;



	//  Determine if the switch was set, and if so go to demo routine.

	PageTable = (void *) calloc(2, NUMBER_VIRTUAL_PAGES);
	if ((argc > 1) && (strcmp(argv[1], "sample") == 0)) {


        mmio.Mode = Z502InitializeContext;
		mmio.Field1 = 0;
		mmio.Field2 = (long) SampleCode;
		mmio.Field3 = (long) PageTable;

		MEM_WRITE(Z502Context, &mmio);   // Start of Make Context Sequence
		mmio.Mode = Z502StartContext;
		//Field1 contains the value of the context returned in the last call
		mmio.Field2 = START_NEW_CONTEXT_AND_SUSPEND;
		MEM_WRITE(Z502Context, &mmio);     // Start up the context//

	} // End of handler for sample code - This routine should never return here

    if ((argc > 1) && (strcmp(argv[1], "test21") == 0)) {
        OSCreateProcess(test21);
    }
    else if ((argc > 1) && (strcmp(argv[1], "test22") == 0)) {
        OSCreateProcess(test22);
    }
    else if ((argc > 1) && (strcmp(argv[1], "test23") == 0)) {
        OSCreateProcess(test23);
    }
    else if ((argc > 1) && (strcmp(argv[1], "test24") == 0)) {
        OSCreateProcess(test24);
    }
    else if ((argc > 1) && (strcmp(argv[1], "test25") == 0)) {
        OSCreateProcess(test25);
    }
    else if ((argc > 1) && (strcmp(argv[1], "test26") == 0)) {
        OSCreateProcess(test26);
    }
    else if ((argc > 1) && (strcmp(argv[1], "test27") == 0)) {
        OSCreateProcess(test27);
    }
    else if ((argc > 1) && (strcmp(argv[1], "test28") == 0)) {
        OSCreateProcess(test28);
    }
    
    
    
    //  By default test0 runs if no arguments are given on the command line
	//  Creation and Switching of contexts should be done in a separate routine.
	//  This should be done by a "OsMakeProcess" routine, so that
	//  test0 runs on a process recognized by the operating system.

	// to run the test , in here you change the name of test.for example if you want to run test1, you change the test0 to test1
//OSCreateProcess(test24);

}

