# GLM battery-front-end draft — independent validation v0.1

Status: **REVIEWED; arithmetic accepted where listed, design conclusion rejected**

Date: 2026-08-30

Reviewed inputs: repository-root `glm_battery_frontend_draft.md` and
`verify_glm_battery_frontend.py`. Authority remains
[`PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.4.md`](../PCB_CONTROLLER_DESIGN_INPUT_FREEZE_v0.4.md).

Run from repository root:

```sh
python3 hardware/electronics/calculations/validate_glm_battery_frontend_draft.py
```

## 1. Validation result

The draft's UVLO nominal arithmetic, UVLO corner enumeration, RSENSE current-limit
arithmetic, raw `VDS x I` screening, and basic `I^2 R` arithmetic reproduce. They do
not establish that the topology passes the Design Input.

The draft is not suitable for promotion as a formal calculation because it contains
the following material errors or classification mistakes:

1. JMSL0302AU Rev. 2.7 does specify maximum `RDS(on)`: 1.5 mOhm at
   `VGS=10 V, ID=20 A`, and 2.9 mOhm at `VGS=4.5 V, ID=15 A`. The draft's repeated
   statement that only typical values exist is false.
2. LMX5069 `ISENSE=8/10 uA` is printed in the typical column, not as a guaranteed
   maximum. Naming 10 uA `ISENSE_MAX` is unsupported, although its computed voltage
   effect across milliohms is negligible.
3. The JKSEMI PDF includes a time-current curve. What is missing is a guaranteed
   min/max tolerance band and a maximum hot resistance, not the curve itself.
4. Exact TVS, OVLO, PWR, TIMER and branch scope are ordinary Engineering Design
   Decisions while suitable candidates and manufacturer data can be obtained. They
   are not automatically `COMPONENT_DATA_BLOCKER` items.
5. The 54--318 W table describes an intentionally unregulated `VOUT approximately 0`
   screen. It is not the expected LMX5069 fault stress after PWR limiting is selected,
   and must not be used as the QPASS design result.
6. The claimed RSENSE contradiction imposes an extra requirement: guaranteed
   `ICL,max <=10 A`. Freeze v0.3 section 3.2 instead defines the load envelope and
   allows active limiting/shutdown above it. A 6.2 mOhm +/-1% candidate guarantees
   `ICL,min=7.665 A`, so it passes the required 7.5 A pulse; its 10.590 A maximum
   limiting corner is a protection threshold issue, not by itself proof of a Design
   Input conflict.
7. The draft omits MX74610SS's guaranteed reverse-leakage maximum of 180 uA at
   `VANODE-CATHODE=-13.5 V`. Freeze v0.3 section 4 targets no more than 100 uA at an
   inactive external power pin. The complete source-isolation topology must prove
   that this test condition cannot energize the battery port, or the topology needs
   another reverse-blocking element / a Design Input revision.

## 2. Corrected first-pass values

### 2.1 RSENSE

For `RSENSE=6.2 mOhm +/-1%` and LMX `VCL=48/55/65 mV`:

```text
ICL,min = 48 mV / (6.2 mOhm x 1.01) = 7.665 A
ICL,typ = 55 mV / 6.2 mOhm          = 8.871 A
ICL,max = 65 mV / (6.2 mOhm x 0.99) = 10.590 A
```

This is the preferred current-sense value for continued analysis because it preserves
the accepted 7.5 A / 1 s branch waveform at the guaranteed-low threshold. Exact MPN,
power rating, pulse overload, TCR and Kelvin footprint remain to be selected.

### 2.2 PWR and TIMER feasibility window

LMX5069 Eq. 13 gives the minimum recommended power-limit setting at 12.6 V:

```text
PLIM,min = 5 mV x VIN,max / RSENSE
          = 5 mV x 12.6 V / 6.2 mOhm
          = 10.161 W
```

A **provisional calculation point**, not a selected component value, is
`PLIM,nom=14 W`. Eq. 14 then gives:

```text
RPWR = 180000 x RSENSE x
       (PLIM - 1.0 mV x VIN,max / RSENSE)
     = 13.356 kOhm nominal
```

Scaling the nominal point by the published `PWRILM=19/25/31 mV` only as a first-order
screen gives about 10.64--17.36 W. This is not yet a guaranteed full corner model;
`IPWR`, RPWR tolerance and the datasheet equation's offset must be included before
selection.

For `CTIMER=100 nF +/-10%`, using `VTMRH=3.2/3.6/4.0 V` and fault charge current
`95/70/45 uA`:

```text
tFAULT,min = 90 nF x 3.2 V / 95 uA  = 3.032 ms
tFAULT,typ = 100 nF x 3.6 V / 70 uA = 5.143 ms
tFAULT,max = 110 nF x 4.0 V / 45 uA = 9.778 ms
```

Keeping the maximum first pulse below 10 ms permits direct comparison with the
published 10 ms JMSL SOA curve. It does **not** yet prove startup: the unknown UBEC
input capacitance and startup load must complete LMX Eq. 9/10/11. This remains
PVR-014 evidence, not a new CDB merely because the external UBEC lacks a datasheet.

### 2.3 Fully-enhanced MOSFET conduction

Use QREV's maximum 2.9 mOhm at 4.5 V because MX74610's gate drive is approximately
5 V; use QPASS's maximum 1.5 mOhm at 10 V because LMX5069 specifies 10--14 V
`GATE-OUT`. For screening at the 100 C junction design boundary, apply a conservative
1.5 multiplier read from JMSL Figure 4. The curve is typical, so assembled-board
thermal validation remains mandatory.

At 6 A:

```text
P_QREV  = 6^2 x 2.9 mOhm x 1.5 = 0.1566 W
P_QPASS = 6^2 x 1.5 mOhm x 1.5 = 0.0810 W
```

Using `RthetaJA,max=65 C/W` as a conservative standalone screen at `TA=40 C` gives
about 50.2 C and 45.3 C respectively, far below the 100 C steady-state boundary.
The result demonstrates enhanced-state conduction feasibility only; the PWR-limited
linear pulse still requires SOA review.

### 2.4 Fuse

At 40 C the datasheet derating graph is approximately 0.97, so the 10 A fuse's
graph-screened rating is about 9.7 A and the 8 A waveform is about 0.825 of that.
The table also guarantees at least 4 h at 1.0 x rated current. This is positive
analytical evidence against a 1 s nuisance opening; it is stronger than the GLM
draft's `I^2t` comparison, because the published 20 A^2s value is a typical
10 x In pre-arcing datum and is not a no-blow threshold.

The fuse is analytically feasible for 6 A continuous and 8 A / 1 s, but final status
remains prototype thermal/no-nuisance validation because resistance is typical,
the curve has no guaranteed tolerance band, and repeated heating depends on PCB copper.

## 3. Review disposition

- **Arithmetic validation:** PASS for the listed UVLO/RSENSE/raw-power numbers.
- **Source interpretation:** FAIL until the seven findings above are corrected.
- **Topology acceptance:** NOT ESTABLISHED.
- **Next calculation:** finish the guaranteed PWR corner model, quantify the 10 ms
  JMSL SOA margin at 12.6 V, bound allowed UBEC input capacitance for the TIMER choice,
  and close the inactive-port reverse-current path before selecting TVS/RPWR/CTIMER MPNs.

This review authorizes no schematic, ERC, netlist, PCB layout, purchase or physical test.
