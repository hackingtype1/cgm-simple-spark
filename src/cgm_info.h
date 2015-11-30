#pragma once
#include <pebble.h>

struct CgmLayer;

typedef struct CgmLayer CgmLayer;
typedef struct CgmData CgmData;

void cgm_data_init(
  unsigned int intTrend,
  unsigned int intNoise,
  char* strDeltaTime,
  char* strEgv,
  char* strDeltaEgv,
  char* strPwdName);
  
CgmData* cgm_data_create(
  unsigned int intTrend,
  unsigned int intNoise,
  char* strDeltaTime,
  char* strEgv,
  char* strDeltaEgv,
  char* strPwdName);
  
CgmLayer* cgm_layer_create(GRect frame);

void cgm_layer_destroy(CgmLayer* layer);
void cgm_layer_set_data(CgmLayer* layer,  const CgmData data);

void cgm_data_set_egv(CgmData* data, char* egv);
char* cgm_data_get_egv(CgmData* data);