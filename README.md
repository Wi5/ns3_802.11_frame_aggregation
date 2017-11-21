# ns3_802.11_frame_aggregation
ns3 scripts for simulating scenarios in which 802.11 frame aggregation is studied


## Citation
If you use this code, please cite the next research article:

Jose Saldana, Jose Ruiz-Mas, Jose Almodovar, "Frame Aggregation in Central Controlled
802.11 WLANs: the Latency vs. Throughput Trade-off," IEEE Communications Letters,
vol.21, no. 2, pp. 2500-2530, Nov. 2017. ISSN 1089-7798.
http://dx.doi.org/10.1109/LCOMM.2017.2741940

http://ieeexplore.ieee.org/document/8013762/

Author's self-archive version: http://diec.unizar.es/~jsaldana/personal/amsterdam_2017_in_proc.pdf


## Content of the repository

The `wifi-central-controlled-aggregation.cc` has more improvements. It contains the ns3 script. It can be run with ns3-27.

The file `wifi-central-controlled-aggregation_v140.cc` is the ns3 file used for the paper. It has been run with ns3-26 (https://www.nsnam.org/ns-3-26/).

The folder `traces` contains the traces used for the video download applications (http://www2.tkn.tu-berlin.de/research/trace/ltvt.html).

The folder `shell_scripts_used_in_the_paper` contains the files used for obtaining each of
the figures presented in the IEEE Communications Letters paper:

- Figure 1
```
test_fig1.1.sh		No aggregation
test_fig1.2.sh		AMPDU aggregation
```

- Figure 2
```
test_fig2.1.sh		No aggregation
test_fig2.2.sh		AMPDU aggregation
```

- Figure 3
```
test_fig3.1.sh		AMPDU 8000
test_fig3.2.sh		AMPDU 16000
```

- Figure 5
```
test_fig5.1_5-25.sh	No aggregation				5, 10, 15, 20 users
test_fig5.1_40-50.sh	No aggregation				40, 50 users
test_fig5.1_80.sh	No aggregation				80 users
test_fig5.2_5-25.sh	AMPDU aggregation			5, 10, 15, 20 users
test_fig5.2_40-50.sh	AMPDU aggregation			40, 50 users
test_fig5.2_80.sh	AMPDU aggregation			80 users
test_fig5.3_5-25.sh	AMPDU aggregation, 8kB			5, 10, 15, 20 users
test_fig5.3_40-50.sh	AMPDU aggregation, 8kB			40, 50 users
test_fig5.3_80.sh	AMPDU aggregation, 8kB			80 users
test_fig5.4_5-25.sh	AMPDU aggregation, Algorithm		5, 10, 15, 20 users
test_fig5.4_40-50.sh	AMPDU aggregation, Algorithm		40, 50 users
test_fig5.4_80.sh	AMPDU aggregation, Algorithm		80 users
test_fig5.5_5-25.sh	AMPDU aggregation, 8kB, Algorithm	5, 10, 15, 20 users
test_fig5.5_40-50.sh	AMPDU aggregation, 8kB, Algorithm	40, 50 users
test_fig5.5_80.sh	AMPDU aggregation, 8kB, Algorithm	80 users
```

## How to use it

- Download ns3.

- Put the `.cc` file in the `ns-3.26/scratch` directory.

- Put a `.sh` file in the `ns-3.26` directory.

- Run a `.sh` file. (You may need to adjust the name of the `.cc` file in the `.sh` script).