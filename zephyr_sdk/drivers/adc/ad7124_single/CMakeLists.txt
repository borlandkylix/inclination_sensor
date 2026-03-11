# Copyright (c) 2021 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0
zephyr_syscall_header(${ZEPHYR_BASE}/include/zephyr/drivers/adc.h)
zephyr_library()
zephyr_library_include_directories(${ZEPHYR_BASE}/drivers/adc)
zephyr_library_sources_ifdef(CONFIG_ADC_AD7124_SINGLE adc_ad7124_single.c)