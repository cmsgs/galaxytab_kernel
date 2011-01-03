#include "INC_INCLUDES.h"

#ifdef INC_EWS_SOURCE_ENABLE


extern ST_TRANSMISSION 	m_ucTransMode;
/***********************************************************************
INC_EWS_INIT()�Լ��� �ݵ�� INC_CHANNEL_START()�Լ� ȣ������ ȣ��Ǿ�� �Ѵ�.                                                     
INC_CHANNEL_START()�� ���ϰ���  INC_SUCCESS�̸�48ms �ֱ�� INC_EWS_FRAMECHECK() 
�Լ��� ȣ���Ѵ�. INC_EWS_FRAMECHECK() == INC_SUCCESS �̸� g_stEWSMsg������ 
���� �����ϰ�, INC_EWS_INIT()�Լ��� ȣ���Ѵ�.                                 

typedef struct _tagST_OUTPUT_EWS
{
	INC_INT16		nNextSeqNo;			Next Segment No 
	INC_INT16		nTotalSeqNo;		Total Segment No
	INC_UINT16		nDataPos;			data posion
	INC_UINT8		ucEWSStartEn;		EWS starting flag
	INC_UINT8		ucIsEWSGood;		EWS Parsing flag

	INC_UINT8		ucMsgGovernment;	�޽��� �߷ɱ��
	INC_UINT8		ucMsgID;			�޽��� ������ȣ
	ST_DATE_T		stDate;				�Ͻ�
	
	INC_INT8		acKinds[4];			�糭 ����
	INC_UINT8		cPrecedence;		�켱 ����
	INC_UINT32		ulTime;				�糭 �ð�
	INC_UINT8		ucForm;				�糭 ��������
	INC_UINT8		ucResionCnt;		�糭 ������
	INC_UINT8		aucResionCode[11];
	INC_DOUBLE32	fEWSCode;
	
	INC_INT8		acOutputBuff[EWS_OUTPUT_BUFF_MAX];	EWS Message ANSI
	
}ST_OUTPUT_EWS, *PST_OUTPUT_EWS;
***********************************************************************/



ST_OUTPUT_EWS	g_stEWSMsg;

ST_OUTPUT_EWS* INC_GET_EWS_DB(void)
{
	return &g_stEWSMsg;
}

INC_UINT32 YMDtoMJD(ST_DATE_T stDate)
{
	INC_UINT16 wMJD;
	INC_UINT32 lYear, lMouth, lDay, L;
	INC_UINT32 lTemp1, lTemp2; 
	
	lYear = (INC_UINT32)stDate.usYear - (INC_UINT32)1900;
	lMouth = stDate.ucMonth;
	lDay = stDate.ucDay;
	
	if(lMouth == 1 || lMouth == 2) L = 1;
	else L = 0;
	
	lTemp1 = (lYear - L) * 36525L / 100L;
	lTemp2 = (lMouth + 1L + L * 12L) * 306001L / 10000L;
	
	wMJD = (INC_UINT16)(14956 + lDay + lTemp1 + lTemp2);

	return wMJD;
}

void MJDtoYMD(INC_UINT16 wMJD, ST_DATE_T *pstDate)
{
	INC_UINT32 lYear, lMouth, lTemp;
	
	lYear = (wMJD * 100L - 1507820L) / 36525L;
	lMouth = ((wMJD * 10000L - 149561000L) - (lYear * 36525L / 100L) * 10000L) / 306001L;
	
	pstDate->ucDay = (INC_UINT8)(wMJD - 14956L - (lYear * 36525L / 100L) - (lMouth * 306001L / 10000L));
	
	if(lMouth == 14 || lMouth == 15) lTemp = 1;
	else lTemp = 0;
	
	pstDate->usYear		= (INC_UINT16)(lYear + lTemp + 1900);
	pstDate->ucMonth	= (INC_UINT8)(lMouth - 1 - lTemp * 12);
}

void INC_EWS_INIT(void)
{
	memset(&g_stEWSMsg, 0 , sizeof(ST_OUTPUT_EWS));
}

void INC_TYPE5_EXTENSION2(ST_FIB_INFO* pFibInfo)
{
	ST_FIG_HEAD*	pHeader;
	ST_TYPE_5*		pType;
	ST_EWS_INFO*	pEwsInfo;
	ST_EWS_TIME*	pstEWSTime;

	INC_UINT16		unData, nLoop;
	INC_UINT32		ulData;
	INC_UINT8		aucInfoBuff[5];

	pHeader = (ST_FIG_HEAD*)&pFibInfo->aucBuff[pFibInfo->ucDataPos++];
	pType	= (ST_TYPE_5*)&pFibInfo->aucBuff[pFibInfo->ucDataPos++];

	if(pType->ITEM.bitD2 == 1)
	{
		unData = INC_GET_WORDDATA(pFibInfo);
		pEwsInfo = (ST_EWS_INFO*)&unData;

		if(!pEwsInfo->ITEM.bitThisSeqNo)
		{
			INC_EWS_INIT();

			for(nLoop = 0; nLoop < 3; nLoop++) g_stEWSMsg.acKinds[nLoop] = INC_GET_BYTEDATA(pFibInfo);
			for(nLoop = 5; nLoop > 0; nLoop--) aucInfoBuff[nLoop-1] = INC_GET_BYTEDATA(pFibInfo);
	
			ulData = ((aucInfoBuff[4]&0x3f)<<24) | (aucInfoBuff[3]<<16) | (aucInfoBuff[2]<<8) | aucInfoBuff[1];
			ulData = ulData >> 2;

			pstEWSTime = (ST_EWS_TIME*)&ulData;

			MJDtoYMD(pstEWSTime->ITEM.bitMJD, &g_stEWSMsg.stDate);
			g_stEWSMsg.stDate.ucHour	= (pstEWSTime->ITEM.bitUTCHours + 9) % 24;
			g_stEWSMsg.stDate.ucMinutes = pstEWSTime->ITEM.bitUTCMinutes;

			g_stEWSMsg.nTotalSeqNo		= pEwsInfo->ITEM.bitTotalNo;
			g_stEWSMsg.ucMsgGovernment	= pEwsInfo->ITEM.bitMsgGovernment;
			g_stEWSMsg.ucMsgID			= pEwsInfo->ITEM.bitID;
			g_stEWSMsg.cPrecedence		= ((aucInfoBuff[4] >> 6) & 0x3);
			g_stEWSMsg.ulTime			= ulData;

			g_stEWSMsg.ucForm			= (((aucInfoBuff[1] & 0x3)<<1) | (aucInfoBuff[0] >> 7));
			g_stEWSMsg.ucResionCnt		= ((aucInfoBuff[0]>>3) & 0xf);
			g_stEWSMsg.nNextSeqNo++;

			if(g_stEWSMsg.nTotalSeqNo)  g_stEWSMsg.ucEWSStartEn	= INC_SUCCESS;

			for(nLoop = 0; nLoop < 10; nLoop++)
				g_stEWSMsg.aucResionCode[nLoop] = INC_GET_BYTEDATA(pFibInfo);

			g_stEWSMsg.fEWSCode = atof((char*)g_stEWSMsg.aucResionCode);

			for( ; nLoop < (pHeader->ITEM.bitLength - 11); nLoop++)
				g_stEWSMsg.acOutputBuff[g_stEWSMsg.nDataPos++] = INC_GET_BYTEDATA(pFibInfo);
		}
		else if(g_stEWSMsg.ucEWSStartEn == INC_SUCCESS)
		{
			if(g_stEWSMsg.nNextSeqNo != pEwsInfo->ITEM.bitThisSeqNo){
				INC_EWS_INIT();
				pFibInfo->ucDataPos += (pHeader->ITEM.bitLength + 1);
				return;
			}

			g_stEWSMsg.nNextSeqNo = pEwsInfo->ITEM.bitThisSeqNo + 1;

			for(nLoop = 0; nLoop < (pHeader->ITEM.bitLength - 3); nLoop++)
				g_stEWSMsg.acOutputBuff[g_stEWSMsg.nDataPos++] = INC_GET_BYTEDATA(pFibInfo);
			
			if(pEwsInfo->ITEM.bitThisSeqNo == g_stEWSMsg.nTotalSeqNo)
				g_stEWSMsg.ucIsEWSGood = INC_SUCCESS;
		}
		else
			pFibInfo->ucDataPos += (pHeader->ITEM.bitLength + 1);
	}
	else 
		pFibInfo->ucDataPos += (pHeader->ITEM.bitLength + 1);
}

void INC_SET_TYPE_5(ST_FIB_INFO* pFibInfo)
{
	ST_TYPE_5*		pExtern;
	ST_FIG_HEAD*	pHeader;
	INC_UINT8		ucType, ucHeader;
	
	ucHeader = INC_GETAT_HEADER(pFibInfo);
	ucType	= INC_GETAT_TYPE(pFibInfo);
	
	pHeader = (ST_FIG_HEAD*)&ucHeader;
	pExtern = (ST_TYPE_5*)&ucType;
	
	switch(pExtern->ITEM.bitExtension){
		case EXTENSION_2: INC_TYPE5_EXTENSION2(pFibInfo); break;
		default: pFibInfo->ucDataPos += (pHeader->ITEM.bitLength + 1); break;
	}
}

INC_UINT8 INC_EWS_PARSING(INC_UINT8* pucFicBuff, INC_INT32 uFicLength)
{
	ST_FIB_INFO* 	pstFib;
	ST_FIG_HEAD* 	pHeader;
	ST_FIC			stEWS;
	INC_UINT8		ucLoop, ucHeader, ucBlockNum;
    INC_UINT16		uiTempIndex = 0;
	
	ucBlockNum = uFicLength / FIB_SIZE;
	pstFib = &stEWS.stBlock;
	
	for(ucLoop = 0; ucLoop < ucBlockNum; ucLoop++)
	{
		INC_SET_UPDATEFIC(pstFib, &pucFicBuff[ucLoop*FIB_SIZE]);
		if(!pstFib->uiIsCRC) continue;
		
		while(pstFib->ucDataPos < FIB_SIZE-2)
		{
			ucHeader = INC_GETAT_HEADER(pstFib);
			pHeader = (ST_FIG_HEAD*)&ucHeader;
			
			if(!INC_GET_FINDTYPE(pHeader) || !INC_GET_NULLBLOCK(pHeader) || !INC_GET_FINDLENGTH(pHeader)) break;
			
			switch(pHeader->ITEM.bitType) {
			case FIG_FICDATA_CHANNEL : INC_SET_TYPE_5(pstFib); break;
			default					 : pstFib->ucDataPos += pHeader->ITEM.bitLength + 1;break;
			}
		}
		if(g_stEWSMsg.ucIsEWSGood == INC_SUCCESS)
			return INC_SUCCESS;
	}

	return INC_ERROR;
}

INC_UINT8 INC_EWS_FRAMECHECK(INC_UINT8 ucI2CID)
{
	INC_UINT16		wFicLen, uFIBCnt;
	INC_UINT8		abyBuff[MAX_FIC_SIZE];
	
	uFIBCnt = INC_GET_FIB_CNT(m_ucTransMode);
	
	if(!(INC_CMD_READ(ucI2CID, APB_VTB_BASE+ 0x00) & 0x4000))
		return INC_ERROR;
	
	wFicLen = INC_CMD_READ(ucI2CID, APB_VTB_BASE+ 0x09);
	if(!wFicLen) return INC_ERROR;
	wFicLen++;
	
	if(wFicLen != (uFIBCnt*FIB_SIZE)) return INC_ERROR;
	INC_CMD_READ_BURST(ucI2CID, APB_FIC_BASE, abyBuff, wFicLen);
	
	if(INC_EWS_PARSING(abyBuff, wFicLen))
		return INC_SUCCESS;

	return INC_ERROR;
}


#endif