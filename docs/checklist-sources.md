# Draft checklist source map

Intent extraction, 2026-10-18. This records why the [checklist](checklist.md) asks each question, not a verdict on current hardware. Read this when interpreting a historical concern or accepted tradeoff; execute against live requirements, datasheets and saved design files.

## Corpus and extraction

Four parallel agents examined three balanced Codex shards and the Claude corpus. The inventory matched session working directories equal to `/Users/mark/git/me/airgrep` or beneath it, and the corresponding Claude project directory; unrelated project histories were outside scope.

- Codex: 130 matching JSONL files, 128 in `~/.codex/sessions/` and 2 in `~/.codex/archived_sessions/`. Seven had no extractable user/assistant conversation; the remaining 123 split into 41 files per agent.
- Claude: 16 JSONL files in `~/.claude/projects/-Users-mark-git-me-airgrep/`, 7 main sessions and 9 delegated subagent transcripts.
- All 139 extracted files were accounted for. Codex metadata identifies 15 primary client sessions, 33 approval-review transcripts and 75 other subagent/fork/replay records; these are not 123 independent human conversations. The current extraction request was excluded from technical evidence.
- Agents read user prompts and later corrections, using surrounding assistant responses to resolve outcomes. Replayed histories and approval-wrapper excerpts were deduplicated; delegated instructions and assistant claims supplied context, not independent user requirements.
- Only textual conversation was normalized. Historical image attachments, tool-result payloads and DFM PDFs were not independently re-inspected, so screenshot-only details and claimed past measurements are not recovered verification evidence.
- Git/LFS maintenance, cached text cleanup and global library installation were excluded as board-review requirements. No historical stock quantity, model output or review verdict is treated as a current fact.

The original six checklist themes remain: ground returns (L01), power integrity (L02), filters (L05), component nonidealities (R04), thermal margins (L06), and implementation/verification (R02, R07, G04, M03). New checks unpack distinct questions and investigation paths without retaining every historical component value.

## Primary source index

References below are original JSONL line numbers, not lines in temporary text extracts. Codex paths are `~/.codex/sessions/YYYY/MM/DD/<filename>` unless marked archived; Claude paths are under the project directory above. Line numbers identify message records; a single JSONL line may contain a long message.

| Key | Original transcript | Relevant human prompts |
|---|---|---|
| C01 | `rollout-2026-10-13T18-12-29-01a13609-6b81-7760-bda7-5f7f86e76829.jsonl`, archived | 38, 246, 342-439: I2C requirements, formulas and guaranteed sink behavior. 681, 721: capacitor roles. 808, 839: heat assumptions and vents. 924-1243: placement, DFM and rules. 1325-1676: exact available replacement parts. |
| C02 | `rollout-2026-10-13T21-50-36-01a136d1-1d49-7cc2-8063-7761601b6f3d.jsonl` | 9, 59, 71, 102, 121, 184, 329, 364: eFuse variant, protection and accepted divider tradeoff. 755-1059: reset coverage, USB case, timing source and simple tactile-button implementation. |
| C03 | `rollout-2026-10-14T20-26-26-01a13baa-6aa7-7730-8d2e-af014ded132b.jsonl` | 60-243: USB source policy, hardware clamp and watchdog registers. 264, 274: accepted residual host-lockup risk. 284, 395: exact controller and bus compatibility. |
| C04 | `rollout-2026-10-15T12-59-26-01a13f37-8794-7343-842a-8677d770a051.jsonl` | 9, 82, 104: power-section complexity, requirement tradeoffs and switching-mode effects. |
| C05 | `rollout-2026-10-15T15-58-53-01a13fdb-d16a-72e0-8560-42faa70c0ec1.jsonl` | 9, 294, 306, 318: feasibility, actual load envelope, thermal/EMI comparison. 868, 888, 1946, 1984: qualify candidates and verify assembly availability. |
| C06 | `rollout-2026-10-15T16-00-47-01a13fdd-8f67-7533-8b65-9b8eafaff35f.jsonl` | 9, 254: bus limits and bandwidth. 388-1114: broader replacement search and FUSB choice. 2007: readable wiring. 2715, 3013, 3103, 3154: actual fixes, model binding and deliberate DRC exclusions. |
| C07 | `rollout-2026-10-17T10-50-06-01a1490d-d658-7390-a55d-04bf816ae9fd.jsonl` | 10: complete independently checked IC inventory. 84: every pin/passive/voltage, per-model author, independent verification, electronics peer review, OK/WARN/ERROR. 1511: prioritize actionable findings. |
| C08 | `rollout-2026-10-17T19-51-09-01a14afd-2f0b-7dd2-96ec-feae901fa27b.jsonl` | 10: gauge isolation proposal to cross-check. 59, 103, 113: distinguish branch current protection from battery fuse and actual demand. 132: hot leakage on CE. |
| C09 | `rollout-2026-10-17T21-08-08-01a14b43-aa79-70f1-aa7d-c351f494c740.jsonl` | 10: copper/model failure. 52-393: corrected module revision/orientation and GPIO remapping. 532-583, 739, 893: pull-ups, expander PWM and shared headers. 942, 1001, 1189-1270: service numbering, DNP values and concise bus docs. 1392-1729: bead behavior, realistic ground-return and resistor review. 1896, 1948, 1962: verify edits and concise conceptual checklist with research guidance. 1993-2159: current limits, TS and BOM consolidation. |
| C10 | `rollout-2026-10-17T21-08-25-01a14b43-eef3-7a33-8f48-92592151dd18.jsonl` | 10, 168: fitted model completeness and supplied geometry. Global library installation itself excluded. |
| C11 | `rollout-2026-10-18T00-22-46-01a14bf5-dec5-7c00-b293-b20bf1806cef.jsonl` | 10, 69: latch and geometry reports. 133-271: BMP scanning, keepout evidence and final disconnected INT. 452, 464: polling and practical re-review. 773-863: USB policy correction and GNSS sequencing. 888, 921: prune stale/unrealistic issues. 945-1242: U.FL, inductor and battery-holder placement corrections. |
| C12 | `rollout-2026-10-18T10-24-47-01a14e1d-0518-71d2-a792-a71b3e7de107.jsonl` | 10, 136: concise current documentation, TI hardware authority, frozen MAX reference. 346: exact switch models. |
| C13 | `rollout-2026-10-18T11-36-15-01a14e5e-7351-70b1-97d2-3071a1587ef2.jsonl` | 10: consolidate RF/compensation capacitors only after datasheet cross-check. |
| H01 | `a7f8869c-58bf-4b2c-a131-919b5ab5c78b.jsonl` | 5, 102, 113, 128: USB/current/bus compatibility and complete dependency updates. 701: readable schematic. 980-1023: actual heat failure. 1049, 1167: rear-side connector markings. |
| H02 | `c7f11619-2635-42e5-b804-9b35f5cbb7cd.jsonl` | 199, 214, 235, 269: simplification, PWM flexibility and cold-start/handover. 326-522: smaller inductors, corrected DCR and board heat model. 990-1476: component metadata and model parity. 1534-1606: per-net return analysis. 1710-2402: soldered GNSS, backup isolation, active/passive antenna, PPS, supervisor, GPIO and readable implementation. |
| H03 | `455b8106-53db-44a4-9f11-ca337ebcbe30.jsonl` | 5, 135, 193, 200: antenna selection, PPS and placement. 305-490, 978, 1065: RF geometry and per-net returns. 1109-1982: fonts, propagation, DFM, fiducials and vias. 2048-3456: exact replacement parts, capacitor/strap roles, Basic parts and per-path OVLO. 3493: actual gates. 3628-3697: realistic receiver context. 3810-4515: bead model evidence and accepted closure. |
| H04 | `a7a8f522-1f89-4149-8aa2-6841bfca7a87.jsonl` | 5, 160, 174: challenge stale review and follow protection chain. 178-299: GNSS fault/isolation claims. 331-484: battery-absent gauge and bus behavior. 544-661: current limits and CE leakage. 665, 721: converter-frequency corner. |
| H05 | `a30b56c5-9f5c-4129-a916-95faa9cfe529.jsonl` | 5, 48: production artifacts and revalidation of review claims. |

## Checklist traceability

Broad pin-review intent C07 supports checks of individual IC requirements. More specific prompts below explain why a topic was retained separately; assistant-generated technical detail remains investigation guidance to verify, not an added user requirement.

| Checklist coverage | Intent sources |
|---|---|
| Review workflow, R02 | C07:10,84; C11:464; C12:136. |
| R01 | C05:294,306; C08:113; H02:235,269. |
| R03, R04, R05 | C07:84; C08:132; C09:1729; H02:326-522; H03:2720,3042. |
| R06 | C11:133-271,452; C09:556. |
| R07 | C06:2007; C09:942,1189; H01:701; H02:2402. |
| R08 | C06:2715; H04:174,178; H03:2720. Interface-specific detailed electrical checks synthesized from these review/part-selection requests. |
| P01, P04, P07, P08 | C03:60-243; C05:9; C07:84; C11:464,816; H01:5,128; H02:235,269. |
| P02, P03 | C03:60-274; C11:773-863; H01:5,102,113. |
| P05 | C02:9-364; H03:2720,3184-3283. |
| P06 | C08:59-113; C09:1993-2159. |
| P09 | C02:755-1059; C11:464. |
| P10 | C11:10; C09:10; H04:5,174. |
| P11 | C09:2063-2159; C07:84; C01:808,839. |
| P12 | C07:84; C08:132; H04:603-661. |
| P13 | C07:84; C02:810; H02:1839,2112. Review synthesis includes checking current requirements/open-work dispositions, not asserting an implemented protection stage. |
| P14 | C05:1984; H02:326-522; H04:665,721. |
| I01, I02, I03 | C01:38-439; C06:9,254; C09:1224,1245; C11:133. |
| I04 | C09:393,532,739,1189; C01:366-386. |
| I05 | C08:10; H04:331-484. |
| I06 | C09:52-393,583,893,1177. |
| I07 | C09:556; C07:84; C11:464; H04:174,178. Physical-control behavior is cross-checked against current requirements, not inferred from an exploratory PWM question. |
| G01 | C11:816,863; H04:228-299. |
| G02 | H02:1821,1839,2112; H03:2048,2201. |
| G03 | H02:1821,2112,2201-2354; H03:5; H04:178. Comparator limits are engineering guidance from the delegated part-selection work, requiring primary-source confirmation. |
| G04 | H03:200,305-490; H02:1710-2402. |
| G05 | H03:135,193; H02:1821,1839. |
| G06 | C11:133-271. |
| L01, L02, L03 | C09:1729,1896; H02:1534-1606; H03:978,1065. |
| L04, L08 | C05:318; C09:1729; H03:200,3628-3697. |
| L05 | C09:1392-1660; H03:3810-4515. |
| L06, L07 | C01:808,839; C09:10; C05:318,1984; H01:980-1023; H02:214,393,522. |
| M01 | C11:69; C01:1143,1188; H03:1982. |
| M02 | C01:1042-1243; C07:84; H03:1867,1982. |
| M03 | C10:10,168; C12:346; H03:490,978; C07:84. |
| M04 | C01:924-1031; C11:945-1242; H03:1808. |
| M05 | C01:1042-1243; H03:1680,1808,1867,3493; H05:5,48. |
| M06 | C09:111,254; H02:1935,2317; H03:5,200; C12:346. |
| M07 | C09:942,1001; H01:1049,1167; H03:1109,1680. |
| M08 | C02:329,364; C05:1946; H03:2528-3456. |
| O01, O02 | C04:9,82; C05:9,318,868,1984; C06:388-1114; H02:199,214,235. |
| O03 | C01:681,721; C09:2063-2159; C13:10; H03:2528-3456. |
| O04 | C11:464,888,921; C12:10,136; C09:2159; H01:128; H03:490,1680. |

## Corrections that change future execution

- **Actual demand:** nominal load, expected peaks and extra short-term headroom are different cases. The historical clarification was 0.8 A nominal, 1 A expected peak and 1.5 A headroom, not 1.5 A continuous duty; current requirements own the numbers. C05:294,306.
- **Practical severity:** a missing guarantee, recommendation deviation, identifiable firmware path and measured failure are different evidence classes. Judge expected-use consequences and recheck stale IC reports before escalating. C07:84; C11:464,888,921.
- **Ground returns:** inspect individual nets and complete supply/return loops with nearby shared stitching. A summed 5 mm detour rule and a via-per-component rule were rejected. H02:1606; H03:1065; C09:1729,1896.
- **Alternative controller:** Plus V1.1 and the intentionally flipped orientation superseded the initial Sense/non-Plus reference. Check each population's bus loading, header mapping and assembly tolerance; fit one controller. C09:254,393,583.
- **I2C:** historical device populations, assumed capacitance and selected pull-up values changed. Recalculate parallel fitted pull-ups and every participant's guaranteed sink/rise-time window; unknown guarantees stay unknown. C01:366-439; C06:254; C09:393,739.
- **BMP580 INT:** leaving INT unconnected superseded grounding it and the explored expander connection. Use the model-specific unused-pin configuration and investigate the reserved-address scan behavior without confusing a device address with a register. C11:133-271.
- **USB policy:** a blanket 500 mA USB-A limit and unconditional lower-of-CC-and-BC1.2 rule were corrected. Detection readiness precedes first access; evaluate the current source mode and firmware specification. C11:773-863. Source-level detail originated in independent re-review, then the user requested the correction.
- **Host lockup:** residual ESP-lockup risk was explicitly accepted after investigation. Review preboot and ordinary detach/change behavior, but a missing independent recovery mechanism is not automatically a new architecture blocker. C03:264,274.
- **Protection:** distinguish exact L/A suffix, fault type, protected path and upstream battery fuse. The early precision-divider target was relaxed toward Basic parts and nominal trip tolerance; later USB-side divider discussion had a different voltage envelope, so never apply one historical window to every feed. C02:121,329,364; C08:103,113; H03:3184-3283.
- **Reset:** accepted QON-based recovery does not promise POR of every IC. Charger/gauge/backup domains may remain powered, and both USB and battery cases need explicit coverage; simple hand-fitted tactile control replaced an expensive power-switch proposal. C02:755-1059.
- **GNSS:** active/passive antenna selection, runtime supervisor setup without OTP, no charging between main and primary backup cells, and PPS indication without input loading remain separate checks. Optional P3 PPS readback was unnecessary because a dedicated PPS path exists. H02:1821-2402; H03:135,193; H04:248-299.
- **RF and thermal models:** use current band/load/enclosure targets; historical thermal-floor limits and two-vent geometry were superseded. Forced PWM was relaxed during candidate searches, while candidate board reuse, unspecified PFM timing and missing bias curves limit proof. C01:839; C05:318; H02:214; H03:3697.
- **Bead disposition:** the specific FB1 pessimistic-corner concern was closed with user acceptance, not manufacturer proof of bias independence. Retain model uncertainty, and reopen only when new evidence changes the accepted basis. H03:4515; C09:1660.
- **Part equivalence:** actual DCR replaced an erroneous distributor value; FUSB detection input and mode straps have different requirements; capacitor consolidation depends on circuit role. Verify exact data rather than retaining an inherited resistor or voltage-rating rule. H02:386; H03:2720,3042; C13:10.
- **Assembly corrections:** use schematic/PCB changes for desired physical orientation and CPL corrections for supplier-origin differences. J7's first offset was rejected before a later adjustment was accepted; importing the first chat correction would reproduce the wrong preview. C11:1076,1101,1217,1242.
- **Rules and closure:** exclusions were deliberate; fixing a required gate includes model bindings and cannot be replaced by weakening rules. Retire resolved open work, preserve physical validation gaps and keep the frozen MAX project historical. C06:3013-3154; C09:2159; C12:136.

## Packaging decision

The checklist is the single executable reference, with a short review sequence and focused domain checks. A new skill would duplicate its routing without adding a distinct workflow, so none was installed; the supplied write-a-skill and writing-for-agents guidance was applied to completion criteria, conditional source references and separation of instructions from provenance. The fresh-context trial tests whether this document is sufficient without the extraction conversation.
