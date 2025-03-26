# CY8CKIT-062S2-AI-PASSTHROUGH

Simple ModusToolbox firmware for the CY8CKIT-062S2-AI to passthrough the Radar (and SPI) via P9.7 to P9.2 on the external header.

Mapping: 
- RSPI_MOSI -> P9.7
- RSPI_MISO -> P9.6
- RSPI_CLK -> P9.5
- RSPI_CS -> P9.4
- RSPI_IRQ -> P9.3
- RXRES_L -> P9.2