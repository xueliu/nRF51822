var general_libraries =
[
    [ "Transport Services", "transport_libraries.html", "transport_libraries" ],
    [ "SoftDevice Handler Library", "lib_softdevice_handler.html", [
      [ "Expectations from the application/developer using this library", "lib_softdevice_handler.html#expectations", [
        [ "Preprocessor defines", "lib_softdevice_handler.html#compiler_defines", null ],
        [ "Subscription to events", "lib_softdevice_handler.html#function_defines", null ],
        [ "No direct calls to sd_softdevice_disable and sd_softdevice_enable", "lib_softdevice_handler.html#api_usage", null ]
      ] ]
    ] ],
    [ "Button handling library", "lib_button.html", null ],
    [ "FIFO library", "lib_fifo.html", [
      [ "Initializing FIFO", "lib_fifo.html#lib_app_fifo_init", null ],
      [ "FIFO instance", "lib_fifo.html#lib_app_fifo_struct", null ],
      [ "Adding an element", "lib_fifo.html#lib_app_fifo_put", null ],
      [ "Fetching an element", "lib_fifo.html#lib_app_fifo_get", null ],
      [ "Empty Buffer", "lib_fifo.html#lib_app_fifo_empty", null ],
      [ "Full Buffer", "lib_fifo.html#lib_app_fifo_full", null ]
    ] ],
    [ "Schedule handling library", "lib_scheduler.html", [
      [ "Requirements:", "lib_scheduler.html#app_scheduler_req", [
        [ "Logic in main context:", "lib_scheduler.html#main_context_logic", null ],
        [ "Logic in interrupt context:", "lib_scheduler.html#int_context_logic", null ]
      ] ],
      [ "Applications using the Scheduler", "lib_scheduler.html#seq_diagrams_sched", null ],
      [ "Applications not using the Scheduler", "lib_scheduler.html#seq_diagrams_no_sched", null ]
    ] ],
    [ "Timer library", "lib_timer.html", null ],
    [ "GPIOTE handling library", "lib_gpiote.html", [
      [ "Initializing GPIOTE module", "lib_gpiote.html#lib_app_gpiote_init", null ],
      [ "Registering with GPIOTE", "lib_gpiote.html#lib_app_gpiote_register", null ],
      [ "Enable/Disable GPIOTE", "lib_gpiote.html#lib_app_gpiote_enable_disable", null ],
      [ "Reading GPIOTE State", "lib_gpiote.html#lib_gpiote_get_states", null ]
    ] ],
    [ "Flash handling library", "lib_flash.html", null ],
    [ "Persistent Storage Manager", "lib_pstorage.html", [
      [ "Introduction", "lib_pstorage.html#pstorage_intro", null ],
      [ "Application Interface Overview", "lib_pstorage.html#pstorage_api_overview", [
        [ "Initialization", "lib_pstorage.html#pstorage_init", null ],
        [ "Registration", "lib_pstorage.html#pstorage_registration", null ],
        [ "Get Block Identifier", "lib_pstorage.html#pstorage_get_block_id", null ],
        [ "Load Data", "lib_pstorage.html#pstorage_load", null ],
        [ "Store Data", "lib_pstorage.html#pstorage_store", null ],
        [ "Update Data", "lib_pstorage.html#pstorage_update", null ],
        [ "Clear Data", "lib_pstorage.html#pstorage_clear", null ],
        [ "Get Status", "lib_pstorage.html#pstorage_status_get", null ]
      ] ],
      [ "Raw Mode", "lib_pstorage.html#pstorage_raw_mode", null ],
      [ "Specifics and limitations of the SDK implementation", "lib_pstorage.html#pstorage_implement_specifics", null ]
    ] ],
    [ "Debug Logger", "lib_trace.html", [
      [ "Initialization", "lib_trace.html#lib_trace_init", null ],
      [ "Debug Logging", "lib_trace.html#lib_logger_log", null ],
      [ "Usage by SDK modules and recommendations", "lib_trace.html#lib_trace_usage", null ]
    ] ]
];