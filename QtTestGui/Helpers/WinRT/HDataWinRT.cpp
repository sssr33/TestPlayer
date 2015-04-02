#if INCLUDE_PCH_H == 1
#include "pch.h"
#endif
#include "HDataWinRT.h"

#if HAVE_WINRT == 1
using namespace Windows::Data::Json;

JsonArray ^HDataWinRT::SavePoint(float x, float y){
	JsonArray ^jsonPoint = ref new JsonArray;

	jsonPoint->Append(JsonValue::CreateNumberValue(x));
	jsonPoint->Append(JsonValue::CreateNumberValue(y));

	return jsonPoint;
}

void HDataWinRT::LoadPoint(JsonArray ^json, float *x, float *y){
	if (x){
		*x = static_cast<float>(json->GetNumberAt(0));
	}

	if (y){
		*y = static_cast<float>(json->GetNumberAt(1));
	}
}
#endif