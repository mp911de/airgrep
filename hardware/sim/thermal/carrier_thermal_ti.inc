* Carrier (BQ25798 variant) thermal charge cut-off and enable chain, as drawn on the sheets
* charger.kicad_sch and battery_control.kicad_sch (net and reference names match).
*
* The ICs are not simulated. Only the pins that load this network are represented:
*   U4 BQ25798   REGN as an ideal source, TS as a leakage current (assumed +-1 uA, not specified),
*                CE as a logic input (VIH 1.3 V min, VIL 0.4 V max, 1 uA)
*   U6 TPS61088  EN as its 800 kOhm pull-down
*   U7 TPS22975  ON draws no current
*   U5 TLV840    RESET open drain, released (VSYS above 2.9 V)
* The benches evaluate V(BAT_TS)/V(CHG_REGN) against the BQ25798 TS thresholds (percent of REGN)
* and V(CHG_DIS) against the CE thresholds, V(PWR_EN) against the EN/ON thresholds.
*
* The benches provide: VSYS, the thermostat between THERM_FEED and THERM_OK, the slider
* between THERM_OK and SW_EN, and node TC (thermistor temperature in deg C as a voltage).

* One .param per line: run_thermal.py overrides them by name.
* REGN LDO, 4.6 to 5.0 V. The thresholds are ratiometric, so its value only scales leakage effects.
.param vregn=4.8
* TS divider from REGN: RT1 (R59) 82 kOhm 1 %, RT2 (R60) 768 kOhm 1 %: nominal window 2.7 to 42.6 C.
* TS leakage and R60 temperature drift remain qualification assumptions.
.param rt1=82k
.param rt2=768k
* Thermostat network. R43 limits the current into a shorted thermostat lead.
.param r43=1k
.param r10=1Meg
.param r12=100k
* Thermistor: R25 (1e12 = unplugged, 1 = shorted), R25 deviation, temperature offset
* (interchangeability, K) and B deviation
.param ntc_r25=100k
.param ntc_rtol=0
.param ntc_dt=0
.param ntc_btol=0
* Leakage currents. ithm > 0 flows out of the TS node into the pin, < 0 into the node.
* The Q2 leakage (IDSS) is constant down to about 0.1 V across the FET.
.param ithm=0
.param q2_idss=0
* FET threshold, see models/fets.lib (q6 only for fet_check.cir)
.param q2_vth=0.9
.param q6_vth=2.0

.include ../models/fets.lib
.include ../models/ntc_entc_eb_100k4262.lib

* --- charger sheet
Vregn CHG_REGN 0 {vregn}
R59   CHG_REGN BAT_TS {rt1}
R60   BAT_TS 0 {rt2}
Ithm  BAT_TS 0 {ithm}
* J2: thermistor on the pack. C35 (10 nF from TS to GND) is not modelled: it filters RF
* pickup on the leads and does not move the DC thresholds evaluated here.
Xntc  BAT_TS 0 TC NTC_ENTC_EB_100K4262 r25={ntc_r25} rtol={ntc_rtol} dt={ntc_dt} btol={ntc_btol}

* --- battery_control sheet: R43 feeds the thermostat lead (J4 pin 1) from VSYS; Q2 drives CE (CHG_DIS)
R43   VSYS THERM_FEED {r43}
R10   THERM_OK 0 {r10}
C31   THERM_OK 0 100n
MQ2   CHG_DIS THERM_OK VSYS Q_AO3401A
Bq2   VSYS CHG_DIS I = {q2_idss} * tanh(V(VSYS,CHG_DIS)/0.1)
R12   CHG_DIS 0 {r12}
* CE pin: 1 uA high-level leakage into the pin
Bce   CHG_DIS 0 I = 1u * (V(CHG_DIS) > 1.3 ? 1 : V(CHG_DIS)/1.3)
R13   SW_EN PWR_EN 100k

* --- boost_switch sheet, U6 EN pull-down
Ren   PWR_EN 0 800k
Cen   PWR_EN 0 10p

* TS as the charger sees it
Bratio RATIO 0 V = V(BAT_TS)/V(CHG_REGN)
