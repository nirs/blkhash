<!--
SPDX-FileCopyrightText: Red Hat Inc
SPDX-License-Identifier: LGPL-2.1-or-later
-->

# Test results on Lenovo ThinkPad P1 Gen 3

## Testing environment

This laptop has 6 cores and 12 threads:

```
$ grep 'model name' /proc/cpuinfo | head -1
model name	: Intel(R) Core(TM) i7-10850H CPU @ 2.70GHz

$ nproc
12
```

Some cores are actually s sibling thread:

```
$ cat /sys/devices/system/cpu/cpu[0-5]/topology/thread_siblings_list
0,6
1,7
2,8
3,9
4,10
5,11
```

This system uses dynamic frequency scaling to save power but also to avoid
over heating, which leads to inconsistent tests results.

```
$ cpupower frequency-info
analyzing CPU 4:
  driver: intel_pstate
  CPUs which run at the same hardware frequency: 4
  CPUs which need to have their frequency coordinated by software: 4
  maximum transition latency:  Cannot determine or is not supported.
  hardware limits: 800 MHz - 5.10 GHz
  available cpufreq governors: performance powersave
  current policy: frequency should be within 800 MHz and 5.10 GHz.
                  The governor "powersave" may decide which speed to use
                  within this range.
  current CPU frequency: Unable to call hardware
  current CPU frequency: 800 MHz (asserted by call to kernel)
  boost state support:
    Supported: yes
    Active: yes
```

## Testing with Hyper-Threading

In this session we run with all CPU threads siblings.

We change the governor to "performance" and limit dynamically frequency
scaling to 3.6 GHz:

```
$ sudo cpupower frequency-set --governor performance --min 3600000kHz --max 5100000kHz

$ cpupower frequency-info -o
          minimum CPU frequency  -  maximum CPU frequency  -  governor
CPU  0      3600000 kHz ( 70 %)  -    5100000 kHz (100 %)  -  performance
CPU  1      3600000 kHz ( 70 %)  -    5100000 kHz (100 %)  -  performance
CPU  2      3600000 kHz ( 70 %)  -    5100000 kHz (100 %)  -  performance
CPU  3      3600000 kHz ( 70 %)  -    5100000 kHz (100 %)  -  performance
CPU  4      3600000 kHz ( 70 %)  -    5100000 kHz (100 %)  -  performance
CPU  5      3600000 kHz ( 70 %)  -    5100000 kHz (100 %)  -  performance
CPU  6      3600000 kHz ( 70 %)  -    5100000 kHz (100 %)  -  performance
CPU  7      3600000 kHz ( 70 %)  -    5100000 kHz (100 %)  -  performance
CPU  8      3600000 kHz ( 70 %)  -    5100000 kHz (100 %)  -  performance
CPU  9      3600000 kHz ( 70 %)  -    5100000 kHz (100 %)  -  performance
CPU 10      3600000 kHz ( 70 %)  -    5100000 kHz (100 %)  -  performance
CPU 11      3600000 kHz ( 70 %)  -    5100000 kHz (100 %)  -  performance
```

Test results:

```
$ test/bench-data.py; test/bench-zero.py; test/bench-hole.py; test/bench-null.py

blkhash-bench --digest-name sha256 --input-type data

 1 threads, 64 streams: 1.10 GiB in 2.002 s (563.56 MiB/s)
 2 threads, 64 streams: 2.15 GiB in 2.003 s (1.07 GiB/s)
 4 threads, 64 streams: 4.09 GiB in 2.002 s (2.04 GiB/s)
 8 threads, 64 streams: 4.31 GiB in 2.002 s (2.15 GiB/s)
16 threads, 64 streams: 6.08 GiB in 2.005 s (3.03 GiB/s)

blkhash-bench --digest-name sha256 --input-type data --aio

 1 threads, 64 streams: 1.10 GiB in 2.002 s (564.44 MiB/s)
 2 threads, 64 streams: 2.16 GiB in 2.002 s (1.08 GiB/s)
 4 threads, 64 streams: 4.08 GiB in 2.002 s (2.04 GiB/s)
 8 threads, 64 streams: 4.28 GiB in 2.002 s (2.14 GiB/s)
16 threads, 64 streams: 5.98 GiB in 2.001 s (2.99 GiB/s)

blkhash-bench --digest-name sha256 --input-type zero

 1 threads, 64 streams: 91.84 GiB in 2.030 s (45.23 GiB/s)
 2 threads, 64 streams: 93.21 GiB in 2.018 s (46.20 GiB/s)
 4 threads, 64 streams: 97.14 GiB in 2.010 s (48.32 GiB/s)
 8 threads, 64 streams: 91.22 GiB in 2.008 s (45.43 GiB/s)
16 threads, 64 streams: 98.06 GiB in 2.008 s (48.83 GiB/s)

blkhash-bench --digest-name sha256 --input-type zero --aio

 1 threads, 64 streams: 68.75 GiB in 2.000 s (34.37 GiB/s)
 2 threads, 64 streams: 119.49 GiB in 2.000 s (59.74 GiB/s)
 4 threads, 64 streams: 224.18 GiB in 2.000 s (112.08 GiB/s)
 8 threads, 64 streams: 234.58 GiB in 2.000 s (117.28 GiB/s)
16 threads, 64 streams: 114.14 GiB in 2.000 s (57.06 GiB/s)

blkhash-bench --digest-name sha256 --input-type hole

 1 threads, 64 streams: 1.88 TiB in 2.059 s (932.31 GiB/s)
 2 threads, 64 streams: 3.56 TiB in 2.036 s (1.75 TiB/s)
 4 threads, 64 streams: 6.81 TiB in 2.032 s (3.35 TiB/s)
 8 threads, 64 streams: 7.50 TiB in 2.045 s (3.67 TiB/s)
16 threads, 64 streams: 9.44 TiB in 2.043 s (4.62 TiB/s)

blkhash-bench --digest-name null --input-type data

 1 threads, 64 streams: 12.89 GiB in 2.000 s (6.44 GiB/s)
 2 threads, 64 streams: 45.02 GiB in 2.000 s (22.51 GiB/s)
 4 threads, 64 streams: 42.62 GiB in 2.000 s (21.31 GiB/s)
 8 threads, 64 streams: 32.79 GiB in 2.000 s (16.40 GiB/s)
16 threads, 64 streams: 32.68 GiB in 2.000 s (16.34 GiB/s)

blkhash-bench --digest-name null --input-type data --aio

 1 threads, 64 streams: 381.04 GiB in 2.000 s (190.51 GiB/s)
 2 threads, 64 streams: 402.69 GiB in 2.000 s (201.34 GiB/s)
 4 threads, 64 streams: 258.21 GiB in 2.000 s (129.10 GiB/s)
 8 threads, 64 streams: 155.29 GiB in 2.000 s (77.64 GiB/s)
16 threads, 64 streams: 147.75 GiB in 2.000 s (73.87 GiB/s)

blkhash-bench --digest-name null --input-type zero

 1 threads, 64 streams: 100.78 GiB in 2.002 s (50.35 GiB/s)
 2 threads, 64 streams: 95.95 GiB in 2.001 s (47.96 GiB/s)
 4 threads, 64 streams: 100.10 GiB in 2.000 s (50.04 GiB/s)
 8 threads, 64 streams: 98.77 GiB in 2.000 s (49.38 GiB/s)
16 threads, 64 streams: 101.31 GiB in 2.000 s (50.65 GiB/s)

blkhash-bench --digest-name null --input-type zero --aio

 1 threads, 64 streams: 73.42 GiB in 2.000 s (36.71 GiB/s)
 2 threads, 64 streams: 125.25 GiB in 2.000 s (62.62 GiB/s)
 4 threads, 64 streams: 228.20 GiB in 2.000 s (114.10 GiB/s)
 8 threads, 64 streams: 234.35 GiB in 2.000 s (117.16 GiB/s)
16 threads, 64 streams: 115.91 GiB in 2.000 s (57.95 GiB/s)

blkhash-bench --digest-name null --input-type hole

 1 threads, 64 streams: 63.31 TiB in 2.001 s (31.65 TiB/s)
 2 threads, 64 streams: 126.25 TiB in 2.001 s (63.10 TiB/s)
 4 threads, 64 streams: 237.81 TiB in 2.001 s (118.85 TiB/s)
 8 threads, 64 streams: 370.94 TiB in 2.001 s (185.38 TiB/s)
16 threads, 64 streams: 516.94 TiB in 2.001 s (258.34 TiB/s)
```

## Testing without Hyper-Threading

In this session we disable the thread siblings using:

```
$ for i in $(seq 6 11); do echo 0 | sudo tee /sys/devices/system/cpu/cpu$i/online; done
0
0
0
0
0
0
```

We change the governor to `performance` and limit dynamically frequency
scaling to 3.6 GHz:

```
$ sudo cpupower frequency-set --governor performance --min 3600000kHz --max 5100000kHz

$ cpupower frequency-info -o
          minimum CPU frequency  -  maximum CPU frequency  -  governor
CPU  0      3600000 kHz ( 70 %)  -    3600000 kHz ( 70 %)  -  performance
CPU  1      3600000 kHz ( 70 %)  -    3600000 kHz ( 70 %)  -  performance
CPU  2      3600000 kHz ( 70 %)  -    3600000 kHz ( 70 %)  -  performance
CPU  3      3600000 kHz ( 70 %)  -    3600000 kHz ( 70 %)  -  performance
CPU  4      3600000 kHz ( 70 %)  -    3600000 kHz ( 70 %)  -  performance
CPU  5      3600000 kHz ( 70 %)  -    3600000 kHz ( 70 %)  -  performance
```

Test results:

```
$ test/bench-data.py; test/bench-zero.py; test/bench-hole.py; test/bench-null.py

blkhash-bench --digest-name sha256 --input-type data

 1 threads, 64 streams: 1.10 GiB in 2.003 s (560.79 MiB/s)
 2 threads, 64 streams: 2.13 GiB in 2.002 s (1.06 GiB/s)
 4 threads, 64 streams: 4.02 GiB in 2.002 s (2.01 GiB/s)
 8 threads, 64 streams: 5.56 GiB in 2.003 s (2.78 GiB/s)

blkhash-bench --digest-name sha256 --input-type data --aio

 1 threads, 64 streams: 1.10 GiB in 2.002 s (565.08 MiB/s)
 2 threads, 64 streams: 2.13 GiB in 2.002 s (1.06 GiB/s)
 4 threads, 64 streams: 4.05 GiB in 2.002 s (2.02 GiB/s)
 8 threads, 64 streams: 5.29 GiB in 2.001 s (2.65 GiB/s)

blkhash-bench --digest-name sha256 --input-type zero

 1 threads, 64 streams: 96.59 GiB in 2.037 s (47.42 GiB/s)
 2 threads, 64 streams: 102.16 GiB in 2.022 s (50.53 GiB/s)
 4 threads, 64 streams: 104.02 GiB in 2.012 s (51.69 GiB/s)
 8 threads, 64 streams: 104.71 GiB in 2.012 s (52.04 GiB/s)

blkhash-bench --digest-name sha256 --input-type zero --aio

 1 threads, 64 streams: 68.47 GiB in 2.000 s (34.23 GiB/s)
 2 threads, 64 streams: 119.25 GiB in 2.000 s (59.62 GiB/s)
 4 threads, 64 streams: 215.68 GiB in 2.000 s (107.83 GiB/s)
 8 threads, 64 streams: 93.85 GiB in 2.000 s (46.92 GiB/s)

blkhash-bench --digest-name sha256 --input-type hole

 1 threads, 64 streams: 1.88 TiB in 2.042 s (940.32 GiB/s)
 2 threads, 64 streams: 3.50 TiB in 2.023 s (1.73 TiB/s)
 4 threads, 64 streams: 6.69 TiB in 2.025 s (3.30 TiB/s)
 8 threads, 64 streams: 8.94 TiB in 2.028 s (4.41 TiB/s)

blkhash-bench --digest-name null --input-type data

 1 threads, 64 streams: 13.46 GiB in 2.000 s (6.73 GiB/s)
 2 threads, 64 streams: 49.05 GiB in 2.000 s (24.53 GiB/s)
 4 threads, 64 streams: 46.37 GiB in 2.000 s (23.19 GiB/s)
 8 threads, 64 streams: 42.41 GiB in 2.000 s (21.20 GiB/s)

blkhash-bench --digest-name null --input-type data --aio

 1 threads, 64 streams: 372.87 GiB in 2.000 s (186.43 GiB/s)
 2 threads, 64 streams: 371.40 GiB in 2.000 s (185.69 GiB/s)
 4 threads, 64 streams: 185.88 GiB in 2.000 s (92.93 GiB/s)
 8 threads, 64 streams: 111.36 GiB in 2.000 s (55.67 GiB/s)

blkhash-bench --digest-name null --input-type zero

 1 threads, 64 streams: 104.99 GiB in 2.001 s (52.46 GiB/s)
 2 threads, 64 streams: 103.31 GiB in 2.001 s (51.64 GiB/s)
 4 threads, 64 streams: 103.21 GiB in 2.001 s (51.59 GiB/s)
 8 threads, 64 streams: 103.70 GiB in 2.001 s (51.83 GiB/s)

blkhash-bench --digest-name null --input-type zero --aio

 1 threads, 64 streams: 72.76 GiB in 2.000 s (36.38 GiB/s)
 2 threads, 64 streams: 124.77 GiB in 2.000 s (62.38 GiB/s)
 4 threads, 64 streams: 231.16 GiB in 2.000 s (115.57 GiB/s)
 8 threads, 64 streams: 90.39 GiB in 2.001 s (45.18 GiB/s)

blkhash-bench --digest-name null --input-type hole

 1 threads, 64 streams: 66.38 TiB in 2.001 s (33.17 TiB/s)
 2 threads, 64 streams: 125.19 TiB in 2.001 s (62.55 TiB/s)
 4 threads, 64 streams: 241.38 TiB in 2.001 s (120.65 TiB/s)
 8 threads, 64 streams: 315.25 TiB in 2.001 s (157.52 TiB/s)
```
