# Heating Curve Mapping

This document explains how config.json fields map to the HeatingLogic class.

## Config Mapping (per heating circuit)

These keys live under `heating[0]` and `heating[1]` in config.json.

- `curveMode` (0 = linear, 1 = 4-point) -> `HeatingConfig.active`
- `minFlow` -> `HeatingConfig.tMin`
- `flowMax` -> `HeatingConfig.tMax`
- `gradient` -> `HeatingConfig.linearSlope`
- `roomsetpoint.temp` -> `HeatingConfig.baseTemp`
- `offset` -> `HeatingConfig.linearOffset`
- `hysteresis` -> `HeatingConfig.hysteresis`
- `curvePoints[]` -> `p1..p4` mapping (see below)

### 4-point curve mapping

`curvePoints` are stored sorted by `out` (cold to warm). The firmware maps them to the
4-point curve in warm-to-cold order like this:

- warmest point -> `p1_out`, `p1_flow`
- next -> `p2_out`, `p2_flow`
- next -> `p3_out`, `p3_flow`
- coldest point -> `p4_out`, `p4_flow`

This means the UI table is shown in cold-to-warm order, but the internal mapping
matches the HeatingLogic P1..P4 definition.

## Example config.json snippet

```json
{
  "heating": [
    {
      "curveMode": 1,
      "minFlow": 25,
      "flowMax": 75,
      "gradient": 1.2,
      "exponent": 1.3,
      "roomsetpoint": { "temp": 20 },
      "curvePoints": [
        { "out": -15, "flow": 70 },
        { "out": 0, "flow": 50 },
        { "out": 10, "flow": 35 },
        { "out": 20, "flow": 20 }
      ]
    }
  ]
}
```

## Notes

- `exponent` is used in Linear Mode to shape the curve (radiator characteristics).
- Linear mode uses: `flow = baseTemp + linearSlope * (baseTemp - outside)^(1/exponent) + offset`.
