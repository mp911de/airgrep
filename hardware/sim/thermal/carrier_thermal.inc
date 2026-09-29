* Carrier thermal charge cut-off and enable chain, as drawn on the sheets
* charger.kicad_sch and battery_control.kicad_sch (net and reference names match).
*
* The ICs are not simulated. Only the pins that load this network are represented:
*   U4 MAX77757  PVL as an ideal source, THM as a leakage current (data sheet +-1 uA max)
*   U6 TPS61088  EN as its 800 kOhm pull-down
*   U7 TPS22975  ON draws no current
*   U5 TLV840    RESET open drain, released (VSYS above 2.9 V)
* The benches evaluate V(BAT_TS)/V(CHG_PVL) against the MAX77757 THM thresholds and
* V(PWR_EN) against the EN/ON thresholds.
*
* The benches provide: VSYS, the thermostat between THERM_FEED and THERM_OK, the slider
* between THERM_OK and SW_EN, and node TC (thermistor temperature in deg C as a voltage).

* One .param per line: run_thermal.py overrides them by name.
* PVL voltage. The data sheet gives no value (1.8 V class, 2.2 V absolute maximum).
.param vpvl=1.8
* THM bias, 154 kOhm 1 %
.param r8=154k
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
* Leakage currents. ithm > 0 flows into the THM pin, < 0 out of it. The FET leakages
* (IDSS) are constant down to about 0.1 V across the FET.
.param ithm=0
.param q2_idss=0
.param q6_idss=0
* FET thresholds, see models/fets.lib
.param q2_vth=0.9
.param q6_vth=2.0

.include ../models/fets.lib
.include ../models/ntc_entc_eb_100k4262.lib

* --- charger sheet
Vpvl  CHG_PVL 0 {vpvl}
R8    CHG_PVL BAT_TS {r8}
Ithm  BAT_TS 0 {ithm}
* J2: thermistor on the pack. C35 (10 nF from THM to GND) is not modelled: it filters RF
* pickup on the leads and does not move the DC thresholds evaluated here.
Xntc  BAT_TS 0 TC NTC_ENTC_EB_100K4262 r25={ntc_r25} rtol={ntc_rtol} dt={ntc_dt} btol={ntc_btol}

* --- battery_control sheet
* R43 feeds the user-made thermostat lead (J4 pin 1) from VSYS
R43   VSYS THERM_FEED {r43}
R10   THERM_OK 0 {r10}
C31   THERM_OK 0 100n
MQ2   CHG_DIS THERM_OK VSYS Q_AO3401A
Bq2   VSYS CHG_DIS I = {q2_idss} * tanh(V(VSYS,CHG_DIS)/0.1)
R12   CHG_DIS 0 {r12}
MQ6   BAT_TS CHG_DIS 0 Q_2N7002
Bq6   BAT_TS 0 I = {q6_idss} * tanh(V(BAT_TS)/0.1)
R13   SW_EN PWR_EN 100k

* --- boost_switch sheet, U6 EN pull-down
Ren   PWR_EN 0 800k
Cen   PWR_EN 0 10p

* THM as the charger sees it
Bratio RATIO 0 V = V(BAT_TS)/V(CHG_PVL)
