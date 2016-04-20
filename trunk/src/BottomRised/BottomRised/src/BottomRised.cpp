#include "stdafx.h"
#include "plugin.h"

#include "MemLeaker.h"

#define MIN_SCALE 1
#define MAX_SCALE 10

BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
    switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
		case DLL_PROCESS_DETACH:
			break;
    }
    return TRUE;
}

PDATAIOFUNC	 g_pFuncCallBack;

//获取回调函数
void RegisterDataInterface(PDATAIOFUNC pfn)
{
	g_pFuncCallBack = pfn;
}

//注册插件信息
void GetCopyRightInfo(LPPLUGIN info)
{
	//填写基本信息
	strcpy(info->Name,"已上升过滤");
	strcpy(info->Dy,"US");
	strcpy(info->Author,"john");
	strcpy(info->Period,"短线");
	strcpy(info->Descript,"已上升过滤");
	strcpy(info->OtherInfo,"自定义天数已上升过滤");
	//填写参数信息
	info->ParamNum = 1;
	strcpy(info->ParamInfo[0].acParaName,"比例");
	info->ParamInfo[0].nMin=MIN_SCALE;
	info->ParamInfo[0].nMax=MAX_SCALE;
	info->ParamInfo[0].nDefault=2;
}

////////////////////////////////////////////////////////////////////////////////
//自定义实现细节函数(可根据选股需要添加)

const	BYTE	g_nAvoidMask[]={0xF8,0xF8,0xF8,0xF8};	// 无效数据标志(系统定义)

char* g_nFatherCode[] = { "999999", "399001", "399005", "399006" };
int g_FatherRate[] = {-1, -1, -1, -1};

typedef enum _eFatherCode
{
	EShangHaiZZ,
	EShenZhenCZ,
	EZhongXBZZ,
	EChuangYBZZ,
	EFatherCodeMax
} EFatherCode;

EFatherCode mathFatherCode(char* Code)
{
	if (NULL == Code)
		return EFatherCodeMax;

	EFatherCode eFCode = EFatherCodeMax;
	if (Code[0] == '6')
	{
		eFCode = EShangHaiZZ;
	}
	else if (Code[0] == '3')
	{
		eFCode = EChuangYBZZ;
	}
	else if (strstr(Code, "002") == Code)
	{
		eFCode = EZhongXBZZ;
	}
	else if (Code[0] == '0')
	{
		eFCode = EShenZhenCZ;
	}

	return eFCode;
}

LPHISDAT minClose(LPHISDAT pHisDat, long lDataNum)
{
	if (NULL == pHisDat || lDataNum <= 0)
		return NULL;

	LPHISDAT pMin = pHisDat;
	for (long i = 0; i < lDataNum; i++)
	{
		if (pMin->Close > (pHisDat+i)->Close)
			pMin = pHisDat+i;
	}
	return pMin;
}


BOOL fEqual(float a, float b)
{
	const float c_fJudge = 0.01;
	float fValue = 0.0;

	if (a > b)
		fValue = a - b;
	else 
		fValue = b - a;

	if (fValue > c_fJudge)
		return FALSE;

	return TRUE;
}


BOOL dateEqual(NTime t1, NTime t2)
{
	if (t1.year != t2.year || t1.month != t2.month || t1.day != t2.day)
		return FALSE;

	return TRUE;
}


NTime dateInterval(NTime nLeft, NTime nRight)
{
	NTime nInterval;
	memset(&nInterval, 0, sizeof(NTime));
	
	unsigned int iLeft = 0;
	unsigned int iRight = 0;
	unsigned int iInterval = 0;

	const unsigned int cDayofyear = 365;
	const unsigned int cDayofmonth = 30;

	iLeft = nLeft.year*cDayofyear + nLeft.month*cDayofmonth + nLeft.day;
	iRight = nRight.year*cDayofyear + nRight.month*cDayofmonth + nRight.day;

	iInterval = (iLeft > iRight) ? iLeft - iRight : iRight - iLeft;

	nInterval.year = iInterval / cDayofyear;
	iInterval = iInterval % cDayofyear;
	nInterval.month = iInterval / cDayofmonth;
	iInterval = iInterval % cDayofmonth;
	nInterval.day = iInterval;

	return nInterval;
}


/* 过滤函数 
   返回值：以S和*开头的股票 或者 上市不满一年，返回FALSE，否则返回TRUE
*/
BOOL filterStock(char * Code, short nSetCode, NTime time1, NTime time2, BYTE nTQ)
{
	if (NULL == Code)
		return FALSE;
	
	int iInfoNum = 2;
	LPSTOCKINFO pStockInfo = new STOCKINFO[iInfoNum];
	memset(pStockInfo, 0, iInfoNum*sizeof(STOCKINFO));

	long readnum = g_pFuncCallBack(Code, nSetCode, STKINFO_DAT, pStockInfo, iInfoNum, time1, time2, nTQ, 0);
	if (readnum <= 0)
	{
		delete[] pStockInfo;
		pStockInfo = NULL;
		return FALSE;
	}
	if ('S' == pStockInfo->Name[0] || '*' == pStockInfo->Name[0])
	{
		delete[] pStockInfo;
		pStockInfo = NULL;
		return FALSE;
	}

	NTime startDate, dInterval;
	memset(&startDate, 0, sizeof(NTime));
	memset(&dInterval, 0, sizeof(NTime));

	long lStartDate = pStockInfo->J_start;
	startDate.year = lStartDate / 10000;
	lStartDate = lStartDate % 10000;
	startDate.month = lStartDate / 100;
	lStartDate = lStartDate % 100;
	startDate.day = lStartDate;

	dInterval = dateInterval(startDate, time2);

	if (dInterval.year < 1)
	{
		delete[] pStockInfo;
		pStockInfo = NULL;
		return FALSE;
	}

	delete[] pStockInfo;
	pStockInfo = NULL;

	return TRUE;
}


int calcUppedPercent(char * Code, short nSetCode, short DataType, NTime time1, NTime time2, BYTE nTQ)
{
	int iUppedSpace = -1;

	LPHISDAT pMin = NULL;

	//窥视数据个数
	long datanum = g_pFuncCallBack(Code, nSetCode, DataType, NULL, -1, time1, time2, nTQ, 0);
	if ( 1 > datanum ){
		return iUppedSpace;
	}

	LPHISDAT pHisDat = new HISDAT[datanum];

	long readnum = g_pFuncCallBack(Code, nSetCode, DataType, pHisDat, datanum, time1, time2, nTQ, 0);
	if ( 1 > readnum || readnum > datanum )
	{
		OutputDebugStringA("========= g_pFuncCallBack read error! =========\n");
		delete[] pHisDat;
		pHisDat = NULL;
		return iUppedSpace;
	}
	
	//停牌股不计算直接返回
	LPHISDAT pLate = pHisDat + readnum - 1;
	if (FALSE == dateEqual(pLate->Time, time2)
		|| fEqual(pLate->fVolume, 0.0))
	{
		OutputDebugStringA(Code);
		OutputDebugStringA("====== Stop trading today. \n");
		delete[] pHisDat;
		pHisDat = NULL;
		return iUppedSpace;
	}

	//查找最低收盘价
	pMin = minClose(pHisDat, readnum);

	if (NULL == pMin)
	{
		OutputDebugStringA("========= minClose error! =========\n");
		delete[] pHisDat;
		pHisDat=NULL;
		return iUppedSpace;
	}

	/*计算已上涨百分比*/
	iUppedSpace = int(((pLate->Close - pMin->Close)/pMin->Close) * 100);

	delete[] pHisDat;
	pHisDat=NULL;

	return iUppedSpace;
}


BOOL InputInfoThenCalc1(char * Code,short nSetCode,int Value[4],short DataType,short nDataNum,BYTE nTQ,unsigned long unused) //按最近数据计算
{
	BOOL nRet = FALSE;
	return nRet;
}

BOOL InputInfoThenCalc2(char * Code,short nSetCode,int Value[4],short DataType,NTime time1,NTime time2,BYTE nTQ,unsigned long unused)  //选取区段
{
	BOOL nRet = FALSE;

	if ( Value[0] < MIN_SCALE || Value[0] > MAX_SCALE || NULL == Code )
		goto endCalc2;

	int iFatherRate = 0, iSonRate = 0;

	/* 计算对应大盘已上升百分比 */
	EFatherCode eFCode = mathFatherCode(Code);
	if (EFatherCodeMax == eFCode)
	{
		OutputDebugString(L"========= Didn't find FatherCode for ");
		OutputDebugString((LPCWSTR)Code);
		OutputDebugString(L" =========\n");
		goto endCalc2;
	}
	//判断是否计算过对应指数
	if (g_FatherRate[eFCode] < 0)
	{
		iFatherRate = calcUppedPercent(g_nFatherCode[eFCode], nSetCode, DataType, time1, time2, nTQ);
		if (iFatherRate < 0)
		{
			OutputDebugStringA("=========== father calcUppedPercent error!!\n");
			goto endCalc2;
		}
		g_FatherRate[eFCode] = iFatherRate;
	}
	else
	{
		iFatherRate = g_FatherRate[eFCode];
	}

	if (FALSE == filterStock(Code, nSetCode, time1, time2, nTQ))
	{
		OutputDebugStringA("===== filter stock : ");
		OutputDebugStringA(Code);
		OutputDebugStringA(" =========\n");
		goto endCalc2;
	}

	/* 计算个股已上升百分比 */
	iSonRate = calcUppedPercent(Code, nSetCode, DataType, time1, time2, nTQ);
	if (iSonRate < 0)
	{
		OutputDebugStringA("=========== son calcUppedPercent error!!\n");
		goto endCalc2;
	}
	
	if ( iSonRate*Value[0] <= iFatherRate )
		nRet = TRUE;
	
endCalc2:
	MEMLEAK_OUTPUT();
	return nRet;
}
