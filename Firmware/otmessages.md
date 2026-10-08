# OpenTherm messages 

| class | id  | msg | key                  | SV MV RV    | type        | unit  | description |
| ---   | --- | --- | ---                  | ---         | ---         | ---   | ---         |
| 1     | 0   | R - | status               | * * *       | flag8.flag8 | -     | master / slave status |
| 1     | 1   | - W | ch_set_t             | - * *       | f8.8        | °C    | Control Setpoint (Tset) |
| 2     | 2   |	- W	| master_config_member | - * *       | flag8.u8    | -     | Master configuration / member ID |
| 2     | 3   |	R - | slave_config_member  | * - -       | flag8.u8    | -     | Slave configuration / member ID |
| 3     | 4   | - W | remote_req           | - - -       | u8.u8       | -     | Request / response code |
| 1     | 5   |	R - | fault_flags          | * - -       | flag8.u8    | -     | ASF-flags / OEM-fault-code |
| 5     | 6   |	R -	| rp_flags             | * - -       | flag8.flag8 | -     | Remote-parameter transfer-enable flags |
| 8     | 7	  | - W | cooling_ctrl         | - * *       | f8.8        | %     |	Cooling control signal |
| 1     | 8	  | - W | ch_set_t2            | - * *       | f8.8        | °C    | Control Setpoint 2 (TsetCH2) |
| 8     | 9   |	R -	| tr_override          | * - -       | f8.8        | °C    | Remote Override Room Setpoint |
| 8     | 14  |	- W | max_rel_mod          | - * *       | f8.8        | %     | Maximum relative modulation level setting |
| 8     | 15  |	R -	| max_cap_min_mod      | * - -       | u8.u8       | kW/%  | Maximum boiler capacity & Minimum modulation level |
| 4     | 16  | - W	| room_set_t           | - * *       | f8.8        | °C    | Room Setpoint |
| 4     | 17  | R -	| rel_mod              | * - -       | f8.8        | %     | Relative Modulation Level |
| 4     | 18  | R - | ch_pressure          | * - -       | f8.8        | bar   | CH water pressure |
| 4     | 19  | R - | dhw_flow_rate        | * - -       | f8.8        | l/min | DHW flow rate |

- SV: value present in table slaveValues
- MV: value present in table masterValues
- RV: value present in table roomUnitValues

# classes
- class 1: control & status information
- class 2: configuration information
- class 3: remote request
- class 4: sensor and informational data
- class 5: remote boiler parameters
- class 8: control of special applications