* Module feeds through U11/U12 LM73100. Quasistatic forward regulation and
* reverse blocking, not a vendor macromodel: no turn-on/reverse-trip timing.
* VF = max(VFWD, I*RON). A native diode provides reverse blocking and
* solver limiting; its artificial forward drop is below 0.6 mV at 1.5 A.
* Load and the unknown total module capacitance use the same assumed ADS share.
.param id_ron=44.85m
.param id_vfwd=28.4m
.param ads_share=0.835
.param cinput=100n
C39 RAIL_5V 0 {cinput}
C38 RAIL_5V 0 {cinput}
.model ID_BLOCK D(is=1n n=0.001)
DU11 RAIL_5V ADS_REG ID_BLOCK
BU11 ADS_REG ADS_SENSE V = max(id_vfwd,id_ron*i(VU11))
VU11 ADS_SENSE ADS_5V_D 0
DU12 RAIL_5V ESP_REG ID_BLOCK
BU12 ESP_REG ESP_SENSE V = max(id_vfwd,id_ron*i(VU12))
VU12 ESP_SENSE ESP_5V_D 0
Rleakads ADS_5V_D 0 1G
Rleakesp ESP_5V_D 0 1G
Rads ADS_5V_D ADS_5V {rmod}
Resp ESP_5V_D ESP_5V {rmod}
Cads ADS_5V ADS_C {cmod*ads_share}
Cesp ESP_5V ESP_C {cmod*(1-ads_share)}
Resrads ADS_C 0 {5m/ads_share}
Resresp ESP_C 0 {5m/(1-ads_share)}
Bloadads ADS_5V 0 I = V(LOADF)*ads_share*min(1,max(0,V(ADS_5V)/3))
Bloadesp ESP_5V 0 I = V(LOADF)*(1-ads_share)*min(1,max(0,V(ESP_5V)/3))
* Measurement-only envelopes; neither source supplies the loads.
Bmodule MODULE 0 V = min(V(ADS_5V),V(ESP_5V))
Bmodulemax MODULE_MAX 0 V = max(V(ADS_5V),V(ESP_5V))
