// choice1.cpp : Defines the entry point for the DLL application.
// 插件实例

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
float g_fFatherRate[] = {0.0, 0.0, 0.0, 0.0};

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


BOOL InputInfoThenCalc1(char * Code,short nSetCode,int Value[4],short DataType,short nDataNum,BYTE nTQ,unsigned long unused) //按最近数据计算
{
	BOOL nRet = FALSE;
	return nRet;
}

BOOL InputInfoThenCalc2(char * Code,short nSetCode,int Value[4],short DataType,NTime time1,NTime time2,BYTE nTQ,unsigned long unused)  //选取区段
{
	BOOL nRet = FALSE;

	if ( Value[0] < MIN_SCALE || Value[0] > MAX_SCALE 
		|| NULL == Code )
		goto endCalc2;	

	float fFatherRate = 0.0, fSonRate = 0.0;

	/* 计算对应大盘已上升百分比 */
	{
		EFatherCode eFCode = mathFatherCode(Code);
		if (EFatherCodeMax == eFCode)
		{
			OutputDebugString(L"========= Didn't find FatherCode for ");
			OutputDebugString((LPCWSTR)Code);
			OutputDebugString(L" =========\n");
			goto endCalc2;
		}
		//判断是否计算过对应指数
		if (fEqual(g_fFatherRate[eFCode], 0.0))
		{
			/* 读取对应大盘数据 */
			LPHISDAT pMin = NULL;

			//窥视数据个数
			long datanum = g_pFuncCallBack(g_nFatherCode[eFCode],nSetCode,DataType,NULL,-1,time1,time2,nTQ,0);
			if ( 2 > datanum )
			{
				//OutputDebugString(L"========= g_pFuncCallBack error! =========\n");
				goto endCalc2;
			}
		
			LPHISDAT pHisDat = new HISDAT[datanum];
			memset(pHisDat, 0, datanum*sizeof(HISDAT));

			long readnum = g_pFuncCallBack(g_nFatherCode[eFCode],nSetCode,DataType,pHisDat,datanum,time1,time2,nTQ,0);
			if ( 2 > readnum || readnum > datanum )
			{
				OutputDebugString(L"========= g_pFuncCallBack read error! =========\n");
				delete[] pHisDat;
				pHisDat = NULL;
				goto endCalc2;
			}
		
			//查找最低收盘价
			pMin = minClose(pHisDat, readnum);
			if (NULL == pMin)
			{
				OutputDebugString(L"========= minClose error! =========\n");
				delete[] pHisDat;
				pHisDat = NULL;
				goto endCalc2;
			}
			//计算已上升百分比
			LPHISDAT pLate = pHisDat + readnum - 1;
			fFatherRate = (pLate->Close - pMin->Close)/pMin->Close;

			g_fFatherRate[eFCode] = fFatherRate;

			delete[] pHisDat;
			pHisDat = NULL;
		}
		else
		{
			fFatherRate = g_fFatherRate[eFCode];
		}		
	}

	/* 过滤名称开头为：S和*的股票 */
	{
		LPSTOCKINFO pStockInfo = new STOCKINFO[2];
		memset(pStockInfo, 0, 2*sizeof(STOCKINFO));

		long readnum = g_pFuncCallBack(Code, nSetCode, STKINFO_DAT, pStockInfo, 1, time2, time2, nTQ, 0);
		if (readnum <= 0)
		{
			delete[] pStockInfo;
			pStockInfo = NULL;
			goto endCalc2;
		}
		if ('S' == pStockInfo->Name[0] || '*' == pStockInfo->Name[0])
		{
			delete[] pStockInfo;
			pStockInfo = NULL;
			goto endCalc2;
		}
		delete[] pStockInfo;
		pStockInfo = NULL;
	}

	/* 计算个股已上升百分比 */
	{
		/**读取个股取数据 */
		LPHISDAT pMin = NULL;

		//窥视数据个数
	    long datanum = g_pFuncCallBack(Code,nSetCode,DataType,NULL,-1,time1,time2,nTQ,0);
		if ( 2 > datanum )
		{
			//OutputDebugString(L"========= g_pFuncCallBack error! =========\n");
			goto endCalc2;
		}

		LPHISDAT pHisDat = new HISDAT[datanum];
		memset(pHisDat, 0, datanum*sizeof(HISDAT));

		long readnum = g_pFuncCallBack(Code,nSetCode,DataType,pHisDat,datanum,time1,time2,nTQ,0);
		if ( 2 > readnum || readnum > datanum )
		{
			OutputDebugString(L"========= g_pFuncCallBack read error! =========\n");
			delete[] pHisDat;
			pHisDat = NULL;
			goto endCalc2;
		}
		
		//查找最低收盘价
		pMin = minClose(pHisDat, readnum);
		if (NULL == pMin)
		{
			OutputDebugString(L"========= minClose error! =========\n");
			delete[] pHisDat;
			pHisDat=NULL;
			goto endCalc2;
		}

		/* 计算已上升百分比 */
		LPHISDAT pLate = pHisDat + readnum - 1;
		//停牌的股票忽略
		/*if (!dateEqual(pLate->Time, time2))
		{
			OutputDebugString(L"========= ");
			OutputDebugString((LPCWSTR)Code);
			OutputDebugString(L" Stop Trading.=========\n");

			delete[] pHisDat;
			pHisDat=NULL;
			goto endCalc2;
		}*/

		fSonRate = (pLate->Close - pMin->Close)/pMin->Close;
		delete[] pHisDat;
		pHisDat = NULL;
	}
	
	if ( fSonRate*((float)Value[0]) < fFatherRate )
		nRet = TRUE;
	
endCalc2:
	MEMLEAK_OUTPUT();
	return nRet;
}
