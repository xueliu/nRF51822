var group__app__uart =
[
    [ "app_uart_comm_params_t", "structapp__uart__comm__params__t.html", [
      [ "baud_rate", "structapp__uart__comm__params__t.html#aed401d2ee067394f8996bd5c0f4ff894", null ],
      [ "cts_pin_no", "structapp__uart__comm__params__t.html#afd55bb9fe47cfd5eff8b468083b2dfed", null ],
      [ "flow_control", "structapp__uart__comm__params__t.html#a0f003e30e3587f068bf45fad9f99bbc4", null ],
      [ "rts_pin_no", "structapp__uart__comm__params__t.html#a836f131cb64d3cdf9831c0354483a65f", null ],
      [ "rx_pin_no", "structapp__uart__comm__params__t.html#a30ae0cdf9181248fa08411af42a938a9", null ],
      [ "tx_pin_no", "structapp__uart__comm__params__t.html#a7153a5ce2ab2d164224a7a17d5b1c56c", null ],
      [ "use_parity", "structapp__uart__comm__params__t.html#a0851da050a073c86c13d0734e78f9d97", null ]
    ] ],
    [ "app_uart_buffers_t", "structapp__uart__buffers__t.html", [
      [ "rx_buf", "structapp__uart__buffers__t.html#ab5c77995614a78b4a7baf6f5ab59bb5c", null ],
      [ "rx_buf_size", "structapp__uart__buffers__t.html#a42c5f8c78ee123d84c7de5da43f703e0", null ],
      [ "tx_buf", "structapp__uart__buffers__t.html#a1073bbd41b6d0ddc55e7ec432cf517dd", null ],
      [ "tx_buf_size", "structapp__uart__buffers__t.html#a8a0ff8a830eadfc972a4d44e1340e392", null ]
    ] ],
    [ "app_uart_evt_t", "structapp__uart__evt__t.html", [
      [ "data", "structapp__uart__evt__t.html#ae56c2b716e2c87405f89d31aa1b46b06", null ],
      [ "error_code", "structapp__uart__evt__t.html#ab2aeea864de0050ec22790a186a4e9b1", null ],
      [ "error_communication", "structapp__uart__evt__t.html#ae2f48256edd4052e8273d098bb8249c4", null ],
      [ "evt_type", "structapp__uart__evt__t.html#ae89e1d236d49d18fa5be6fa32ba3c0d8", null ],
      [ "value", "structapp__uart__evt__t.html#a9e34eabccc01fe3eeff1c7392d737d84", null ]
    ] ],
    [ "APP_UART_FIFO_INIT", "group__app__uart.html#ga25b7b23e541732b945b5992b5519a10f", null ],
    [ "APP_UART_INIT", "group__app__uart.html#gab37ec15202892ba42f454daacaa38b7f", null ],
    [ "UART_PIN_DISCONNECTED", "group__app__uart.html#gaa790ff39126a00c4308f30f1139f4efa", null ],
    [ "app_uart_event_handler_t", "group__app__uart.html#gae903f13e59955f64c03db53d9b74ba2e", null ],
    [ "app_uart_connection_state_t", "group__app__uart.html#ga683d8281c00679b243c06fd9b7815557", [
      [ "APP_UART_DISCONNECTED", "group__app__uart.html#gga683d8281c00679b243c06fd9b7815557acb220f64509a4882b0b79f1a4620398c", null ],
      [ "APP_UART_CONNECTED", "group__app__uart.html#gga683d8281c00679b243c06fd9b7815557a026c1999c42fd293cc7ead39f6de1cf2", null ]
    ] ],
    [ "app_uart_evt_type_t", "group__app__uart.html#ga9346b21b144fd9499e24853bbf781e17", [
      [ "APP_UART_DATA_READY", "group__app__uart.html#gga9346b21b144fd9499e24853bbf781e17ad0c6e0d9cef6b81de23e2f2583013ef1", null ],
      [ "APP_UART_FIFO_ERROR", "group__app__uart.html#gga9346b21b144fd9499e24853bbf781e17ac4e8a2753ab64a36a3bd0e723a3f26ea", null ],
      [ "APP_UART_COMMUNICATION_ERROR", "group__app__uart.html#gga9346b21b144fd9499e24853bbf781e17a6ee822a8a07d1be09ac336495a5238d8", null ],
      [ "APP_UART_TX_EMPTY", "group__app__uart.html#gga9346b21b144fd9499e24853bbf781e17a9fa032c57eadcef66e102eb78a04c2d1", null ],
      [ "APP_UART_DATA", "group__app__uart.html#gga9346b21b144fd9499e24853bbf781e17a5b4c2503e1e658ff28dcaad17eff6ac4", null ]
    ] ],
    [ "app_uart_flow_control_t", "group__app__uart.html#gad0b0f33b12902ce08681e06f304f0cba", [
      [ "APP_UART_FLOW_CONTROL_DISABLED", "group__app__uart.html#ggad0b0f33b12902ce08681e06f304f0cbaae7fd58fef6c10140a659be29f0b81b8b", null ],
      [ "APP_UART_FLOW_CONTROL_ENABLED", "group__app__uart.html#ggad0b0f33b12902ce08681e06f304f0cbaa76123f43769a3f237d5a670844c6d235", null ],
      [ "APP_UART_FLOW_CONTROL_LOW_POWER", "group__app__uart.html#ggad0b0f33b12902ce08681e06f304f0cbaa26a16d7573f75722e220954fa4fbf27f", null ]
    ] ],
    [ "app_uart_close", "group__app__uart.html#ga8b5b23229363aefa52be33360dfb5fde", null ],
    [ "app_uart_flush", "group__app__uart.html#ga2b867b8dfc7209b87e2e8f0da0741105", null ],
    [ "app_uart_get", "group__app__uart.html#gacddb5b7b711ef104f9eb181a13bc4503", null ],
    [ "app_uart_get_connection_state", "group__app__uart.html#ga4360a21365ae56f22344f84c20933d64", null ],
    [ "app_uart_init", "group__app__uart.html#gae650b57bf30da0f26ae409782de9fcbd", null ],
    [ "app_uart_put", "group__app__uart.html#ga2e4c8407274a151e72ed5a226529dc36", null ]
];