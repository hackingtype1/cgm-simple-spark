#include "cgm_info.h"

struct CgmData{
  
  unsigned int intTrend;
  unsigned int intNoise;
  char* strDeltaTime;
  char* strEgv;
  char* strDeltaEgv;
  char* strPwdName;

};

CgmData* cgm_data_create(unsigned int intTrend,
  unsigned int intNoise,
  char* strDeltaTime,
  char* strEgv,
  char* strDeltaEgv,
  char* strPwdName) {
  CgmData* data = malloc(sizeof(CgmData));
  data->intTrend = intTrend;
  data->intNoise = intNoise;
  data->strDeltaTime = strDeltaTime;
  data->strEgv = strEgv;
  data->strDeltaEgv = strDeltaEgv;
  data->strPwdName = strPwdName;
  return data;
}

void cgm_data_set_egv(CgmData* data,  char* egv) {
  data->strEgv = egv;
}

char* cgm_data_get_egv(CgmData* data) {
  return data->strEgv;
}