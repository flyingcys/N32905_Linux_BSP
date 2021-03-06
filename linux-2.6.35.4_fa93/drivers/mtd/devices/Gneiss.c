/**
  ******************************************************************************
  * @file    /Gneiss.c
  * @author  Winbond FAE Steam Lin
  * @version V1.1.0
  * @date    09-December-2015
  * @brief   This code provide the low level RPMC hardware operate function based on STM32F205.
  *            
  * COPYRIGHT 2015 Winbond Electronics Corporation.
*/ 

#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include "Secureic.h"

/* public array use for RPMC algorithm */
unsigned char w74m_message[16];	// Message data use for insturction input
unsigned char w74m_counter[4];	// counter data (32bit)
unsigned char w74m_tag[12];		// Tag data use for increase counter
unsigned char w74m_signature[32];	// Signature data use for every instruction output

/********************
Function: Read counter
Argument:
return:	counter number
date: 2015/8/12
*********************/
unsigned int RPMC_ReadCounterData(void)
{
	return (((((w74m_counter[0]*0x100)+w74m_counter[1])*0x100)+w74m_counter[2])*0x100)+w74m_counter[3];
}

/********************
Function: RPMC read RPMC status
Argument: checkall

	checkall = 0: only read out RPMC status
	checkall = 1: Read out counter data, tag, signature information
	
return: RPMC status 
date: 2015/8/12
*********************/
unsigned int RPMC_ReadRPMCstatus(struct spi_device *spi, unsigned int checkall)
{
	unsigned int i;
 	u8 RPMCstatus;
	u8      code[2];
	u8 data[50];

	code[0] = 0x96;
	code[1] = 0x00;
	if(checkall ==0)  {
  		spi_write_then_read(spi, code, 2, &RPMCstatus, 1);	
	}
	else
	{
  		spi_write_then_read(spi, code, 2, data, 49);	
	  	RPMCstatus = data[0];		// status was readout, read RPMC status don't need signature input
		
		for(i=0;i<12;i++){
  			w74m_tag[i]=data[1+i];			// tag information repeat
	   	}
  		for(i=0;i<4;i++){
  			w74m_counter[i] = data[13+i];	// counter data readout
	   	}
    		for(i=0;i<32;i++){
	    		w74m_signature[i] = data[17+i];	// signature repeat
    		}
	}
	return RPMCstatus;
}

/********************
Function: RPMC Update HMAC key, this function should call in every Gneiss power on
Argument: 
	cadr: selected Counter address, from 1~4
	rootkey: rootkey use for generate HMAC key
	hmac4: 4 byte input hmac message data, which can be time stamp, serial number or random number. 
	hmackey: 32 byte HMACKEY, which would be use for increase/request counter after RPMC_UpHMACkey() operation success
return: 
date: 2015/8/12
*********************/
unsigned int RPMC_UpHMACkey(struct spi_device *spi, unsigned int cadr,unsigned char *rootkey,unsigned char *hmac4,unsigned char *hmackey)
{
	unsigned int i;
  	u8      code[40];
	w74m_message[0]=0x9B;
	w74m_message[1]=0x01;
	w74m_message[2]=cadr - 1;
	w74m_message[3]=0x00;
	w74m_message[4]=*(hmac4+0);
	w74m_message[5]=*(hmac4+1);
	w74m_message[6]=*(hmac4+2);
	w74m_message[7]=*(hmac4+3);
  
	hmacsha256(rootkey,32,hmac4,4,hmackey);	// use rootkey generate HMAC key by SHA256  
	hmacsha256(hmackey,32,w74m_message,8,w74m_signature);	// caculate signature by SHA256

	for(i=0;i<8;i++){
  		code[i] = w74m_message[i];
	}
	for(i=0;i<32;i++){
		code[8 + i] = w74m_signature[i];
	}
	spi_write_then_read(spi, code, 40, NULL, 0); 

	while((RPMC_ReadRPMCstatus(spi, 0) & 0x01) == 0x01)
	{
		// wait until RPMC operation done
	}  
	return RPMC_ReadRPMCstatus(spi, 0); 
}

/********************
Function: RPMC request counter data
Argument: 
	cadr: selected Counter address, from 1~4
	hmackey: 32 byte HMACKEY which is generated by RPMC_UpHMACkey()
	input_tag: 12 byte input Tag data, which can be time stamp, serial number or random number. 
	these data would repeat after success RPMC_ReqCounter() operation
return: 
date: 2015/8/12
*********************/
void RPMC_ReqCounter(struct spi_device *spi, unsigned int cadr, unsigned char *hmackey,unsigned char *input_tag)
{
	unsigned int i;
  	u8      code[48];
	w74m_message[0]=0x9B;
	w74m_message[1]=0x03;
	w74m_message[2]=cadr-1;
	w74m_message[3]=0x00;
	for(i=0;i<12;i++)
		w74m_message[i+4]=*(input_tag+i);
  
	hmacsha256(hmackey,32,w74m_message,16,w74m_signature);  // caculate signature by SHA256

  	for(i=0;i<16;i++){
  		code[i] = w74m_message[i];
	}
	for(i=0;i<32;i++){
		code[16 + i] = w74m_signature[i];
	}

	spi_write_then_read(spi, code, 48, NULL, 0); 

	return;
}
/********************
Function: RPMC request counter data
Argument: 
	cadr: selected Counter address, from 1~4
	hmackey: 32 byte HMACKEY which is generated by RPMC_UpHMACkey()
	input_tag: 12 byte input Tag data, which can be time stamp, serial number or random number. 
	these data would repeat after success RPMC_ReqCounter() operation
return: 
date: 2015/8/12
*********************/
unsigned int RPMC_IncCounter(struct spi_device *spi, unsigned int cadr,unsigned char *hmackey,unsigned char *input_tag)
{
	unsigned int i;
  	u8      code[40];
	RPMC_ReqCounter(spi, cadr, hmackey, input_tag);	
	while((RPMC_ReadRPMCstatus(spi, 0) & 0x01) == 0x01)
	{
		// wait until RPMC operation done
	}  
	RPMC_ReadRPMCstatus(spi, 1);	// Get counter information
  
	w74m_message[0]=0x9B;
	w74m_message[1]=0x02;
	w74m_message[2]=cadr-1;
	w74m_message[3]=0x00;
	for(i=0;i<4;i++){
		w74m_message[i+4]=w74m_counter[i];
	}  
  
	hmacsha256(hmackey,32,w74m_message,8,w74m_signature);	// caculate signature by SHA256
  
  	for(i=0;i<8;i++){
  		code[i] = w74m_message[i];
	}
	for(i=0;i<32;i++){
		code[8 + i] = w74m_signature[i];
	}
	spi_write_then_read(spi, code, 40, NULL, 0); 

  	while((RPMC_ReadRPMCstatus(spi, 0) & 0x01) == 0x01)
	{
		// wait until RPMC operation done
	}  
	return RPMC_ReadRPMCstatus(spi, 0); 
}

/********************
Function: RPMC Challenge signature. Main security operation
Argument: 
	cadr: selected Counter address, from 1~4
	hmackey: 32 byte HMACKEY which is generated by RPMC_UpHMACkey()
	input_tag: 12 byte input Tag data, which can be time stamp, serial number or random number.
return: 
	Challlenge result. if signature match, return 0.
date: 2015/12/09
*********************/
unsigned char RPMC_Challenge(struct spi_device *spi, unsigned int cadr, unsigned char *hmackey,unsigned char *input_tag)
{
	unsigned char Verify_signature[32];	// signature for verification. should match signature[32]
	unsigned int i;
	RPMC_ReqCounter(spi, cadr, hmackey, input_tag);
	while((RPMC_ReadRPMCstatus(spi, 0) & 0x01) == 0x01)
	{
		// wait until RPMC operation done
	}  
	RPMC_ReadRPMCstatus(spi, 1);	// Get counter information. In this stage, tag[12], counter[4], signature[32] is updated.
	// Comment: the message using for signature is tag[0:11]+count[0:3] data, you can also use memcpy to casecade these data	
	for(i = 0; i < 12; i++){
		w74m_message[i] = w74m_tag[i];	// message [0:11] = tag[0:11]
	}
	for(i = 0; i < 4; i++){
		w74m_message[12+i] = w74m_counter[i];	// message [12:15] = counter[0:3]
	}
	hmacsha256(hmackey,32,w74m_message,16,Verify_signature);	// Verification signature should as same as security output
	return memcmp(Verify_signature, w74m_signature, 32);		// Compare Verification signature (computed by controllor) and internal signature (return from security Flash by request counter operation)
}
