#include "arg.h"
#include "common.h"
#include "console.h"
#include "llama.h"
#include "log.h"
#include "server-context.h"
#include "server-http.h"
#include "server-models.h"
#include "server-task.h"

#include <signal.h>

#include <atomic>
#include <exception>
#include <thread>  // for std::thread::hardware_concurrency

#if defined(_WIN32)
#    include <windows.h>
#endif

static std::function<void(int)> shutdown_handler;
static std::atomic_flag         is_terminating = ATOMIC_FLAG_INIT;

static std::atomic<bool> g_is_interrupted = false;

static inline void signal_handler(int signal) {
    if (is_terminating.test_and_set()) {
        // in case it hangs, we can force terminate the server by hitting Ctrl+C twice
        // this is for better developer experience, we can remove when the server is stable enough
        fprintf(stderr, "\033[0m\nReceived second interrupt, terminating immediately.\n");
        exit(1);
    }

    g_is_interrupted.store(true);
    shutdown_handler(signal);
}

// wrapper function that handles exceptions and logs errors
// this is to make sure handler_t never throws exceptions; instead, it returns an error response
static server_http_context::handler_t ex_wrapper(server_http_context::handler_t func) {
    return [func = std::move(func)](const server_http_req & req) -> server_http_res_ptr {
        std::string message;
        error_type  error;
        try {
            return func(req);
        } catch (const std::invalid_argument & e) {
            // treat invalid_argument as invalid request (400)
            error   = ERROR_TYPE_INVALID_REQUEST;
            message = e.what();
        } catch (const std::exception & e) {
            // treat other exceptions as server error (500)
            error   = ERROR_TYPE_SERVER;
            message = e.what();
        } catch (...) {
            error   = ERROR_TYPE_SERVER;
            message = "unknown error";
        }

        auto res    = std::make_unique<server_http_res>();
        res->status = 500;
        try {
            json error_data = format_error_response(message, error);
            res->status     = json_value(error_data, "code", 500);
            res->data       = safe_json_to_str({
                { "error", error_data }
            });
            SRV_WRN("got exception: %s\n", res->data.c_str());
        } catch (const std::exception & e) {
            SRV_ERR("got another exception: %s | while handling exception: %s\n", e.what(), message.c_str());
            res->data = "Internal Server Error";
        }
        return res;
    };
}

//
// TUI (Terminal UI) logic
//

struct cli_context {
    server_context &        ctx_server;
    json                    messages = json::array();
    std::vector<raw_buffer> input_files;
    task_params             defaults;
    bool                    verbose_prompt;

    cli_context(const common_params & params, server_context & ctx) : ctx_server(ctx) {
        defaults.sampling    = params.sampling;
        defaults.speculative = params.speculative;
        defaults.n_keep      = params.n_keep;
        defaults.n_predict   = params.n_predict;
        defaults.antiprompt  = params.antiprompt;

        defaults.stream            = true;
        defaults.timings_per_token = true;
        verbose_prompt             = params.verbose_prompt;
    }

    std::string generate_completion(result_timings & out_timings) {
        server_response_reader rd          = ctx_server.get_response_reader();
        auto                   chat_params = format_chat();
        {
            server_task task = server_task(SERVER_TASK_TYPE_COMPLETION);
            task.id          = rd.get_new_id();
            task.index       = 0;
            task.params      = defaults;
            task.cli_prompt  = chat_params.prompt;
            task.cli_files   = input_files;
            task.cli         = true;

            task.params.chat_parser_params                  = common_chat_parser_params(chat_params);
            task.params.chat_parser_params.reasoning_format = COMMON_REASONING_FORMAT_DEEPSEEK;
            if (!chat_params.parser.empty()) {
                task.params.chat_parser_params.parser.load(chat_params.parser);
            }

            rd.post_task({ std::move(task) });
        }

        console::spinner::start();
        auto should_stop = []() {
            return g_is_interrupted.load();
        };
        server_task_result_ptr result = rd.next(should_stop);

        console::spinner::stop();
        std::string curr_content;
        bool        is_thinking = false;

        while (result) {
            if (should_stop()) {
                break;
            }
            if (result->is_error()) {
                json err_data = result->to_json();
                console::error("Error: %s\n", err_data.at("message").get<std::string>().c_str());
                return curr_content;
            }
            auto * res_partial = dynamic_cast<server_task_result_cmpl_partial *>(result.get());
            if (res_partial) {
                out_timings = res_partial->timings;
                for (const auto & diff : res_partial->oaicompat_msg_diffs) {
                    if (!diff.content_delta.empty()) {
                        if (is_thinking) {
                            console::log("\n[End thinking]\n\n");
                            console::set_display(DISPLAY_TYPE_RESET);
                            is_thinking = false;
                        }
                        curr_content += diff.content_delta;
                        console::log("%s", diff.content_delta.c_str());
                        console::flush();
                    }
                    if (!diff.reasoning_content_delta.empty()) {
                        console::set_display(DISPLAY_TYPE_REASONING);
                        if (!is_thinking) {
                            console::log("[Start thinking]\n");
                        }
                        is_thinking = true;
                        console::log("%s", diff.reasoning_content_delta.c_str());
                        console::flush();
                    }
                }
            }
            auto * res_final = dynamic_cast<server_task_result_cmpl_final *>(result.get());
            if (res_final) {
                out_timings = res_final->timings;
                break;
            }
            result = rd.next(should_stop);
        }
        g_is_interrupted.store(false);
        return curr_content;
    }

    common_chat_params format_chat() {
        auto                         meta        = ctx_server.get_meta();
        auto &                       chat_params = meta.chat_params;
        common_chat_templates_inputs inputs;
        inputs.messages              = common_chat_msgs_parse_oaicompat(messages);
        inputs.use_jinja             = chat_params.use_jinja;
        inputs.add_generation_prompt = true;
        inputs.enable_thinking       = chat_params.enable_thinking;
        return common_chat_templates_apply(chat_params.tmpls.get(), inputs);
    }
};

const char * LLAMA_ASCII_LOGO = R"(
▄▄ ▄▄
██ ██
██ ██  ▀▀█▄ ███▄███▄  ▀▀█▄    ▄████ ████▄ ████▄
██ ██ ▄█▀██ ██ ██ ██ ▄█▀██    ██    ██ ██ ██ ██
██ ██ ▀█▄██ ██ ██ ██ ▀█▄██ ██ ▀████ ████▀ ████▀
                                    ██    ██
                                    ▀▀    ▀▀
)";

static void run_tui(common_params & params, server_context & ctx_server) {
    cli_context ctx_cli(params, ctx_server);
    console::init(params.simple_io, params.use_color);

    // Start background inference loop
    std::thread inference_thread([&ctx_server]() { ctx_server.start_loop(); });

    auto inf = ctx_server.get_meta();
    console::log("\n%s\n", LLAMA_ASCII_LOGO);
    console::log("build      : %s\n", inf.build_info.c_str());
    console::log("model      : %s\n", inf.model_name.c_str());
    console::log("\nType /exit or Ctrl+C to quit.\n\n");

    std::string cur_msg;
    while (true) {
        std::string buffer;
        console::set_display(DISPLAY_TYPE_USER_INPUT);
        console::log("> ");
        std::string line;
        bool        another_line = console::readline(line, params.multiline_input);
        buffer += line;
        while (another_line) {
            another_line = console::readline(line, params.multiline_input);
            buffer += line;
        }
        console::set_display(DISPLAY_TYPE_RESET);

        if (g_is_interrupted.load()) {
            g_is_interrupted.store(false);
            break;
        }
        if (buffer.empty()) {
            continue;
        }
        if (buffer == "/exit") {
            break;
        }
        if (buffer == "/clear") {
            ctx_cli.messages.clear();
            console::log("History cleared.\n");
            continue;
        }

        ctx_cli.messages.push_back({
            { "role",    "user" },
            { "content", buffer }
        });
        result_timings timings;
        std::string    assistant_content = ctx_cli.generate_completion(timings);
        ctx_cli.messages.push_back({
            { "role",    "assistant"       },
            { "content", assistant_content }
        });
        console::log("\n");
    }

    ctx_server.terminate();
    inference_thread.join();
    console::cleanup();
}

int main(int argc, char ** argv) {
    // own arguments required by this example
    common_params params;

    if (!common_params_parse(argc, argv, params, LLAMA_EXAMPLE_SERVER)) {
        return 1;
    }

    // Bundle detection
    bool         is_bundle  = false;
    const char * env_source = std::getenv("BAREMETALLAMA_SOURCE");
    const char * env_offset = std::getenv("BAREMETALLAMA_OFFSET");

    if (env_source && env_offset) {
        params.model.path   = env_source;
        params.model.offset = (size_t) std::stoull(env_offset);
        is_bundle           = true;
        LOG_INF("%s: Bundled model detected via environment: %s (offset %zu)\n", __func__, env_source,
                params.model.offset);
    } else if (params.model.path.empty() || params.model.path == "models/7B/ggml-model-f16.gguf") {
        // Try to detect self-bundle
#pragma pack(push, 1)

        struct BundleFooter {
            uint64_t model_offset;
            uint64_t model_size;
            uint32_t magic;
        };

#pragma pack(pop)

        const char * exe_path = "/proc/self/exe";
        FILE *       f        = fopen(exe_path, "rb");
        if (f) {
            fseek(f, -((long) sizeof(BundleFooter)), SEEK_END);
            BundleFooter footer;
            if (fread(&footer, sizeof(footer), 1, f) == 1) {
                if (footer.magic == 0x47475546) {
                    LOG_INF("%s: Bundled model detected at offset %zu\n", __func__, (size_t) footer.model_offset);
                    params.model.path   = exe_path;
                    params.model.offset = (size_t) footer.model_offset;
                    is_bundle           = true;
                }
            }
            fclose(f);
        }
    }

    bool tui_mode = (argc == 1 && is_bundle);

    // validate batch size for embeddings
    // embeddings require all tokens to be processed in a single ubatch
    // see https://github.com/ggml-org/llama.cpp/issues/12836
    if (params.embedding && params.n_batch > params.n_ubatch) {
        LOG_WRN("%s: embeddings enabled with n_batch (%d) > n_ubatch (%d)\n", __func__, params.n_batch,
                params.n_ubatch);
        LOG_WRN("%s: setting n_batch = n_ubatch = %d to avoid assertion failure\n", __func__, params.n_ubatch);
        params.n_batch = params.n_ubatch;
    }

    if (params.n_parallel < 0) {
        LOG_INF("%s: n_parallel is set to auto, using n_parallel = 4 and kv_unified = true\n", __func__);

        params.n_parallel = 4;
        params.kv_unified = true;
    }

    // for consistency between server router mode and single-model mode, we set the same model name as alias
    if (params.model_alias.empty() && !params.model.name.empty()) {
        params.model_alias = params.model.name;
    }

    common_init();

    // struct that contains llama context and inference
    server_context ctx_server;

    llama_backend_init();
    llama_numa_init(params.numa);

    LOG_INF("system info: n_threads = %d, n_threads_batch = %d, total_threads = %d\n", params.cpuparams.n_threads,
            params.cpuparams_batch.n_threads, std::thread::hardware_concurrency());
    LOG_INF("\n");
    LOG_INF("%s\n", common_params_get_system_info(params).c_str());
    LOG_INF("\n");

    server_http_context ctx_http;
    if (!ctx_http.init(params)) {
        LOG_ERR("%s: failed to initialize HTTP server\n", __func__);
        return 1;
    }

    //
    // Router
    //

    // register API routes
    server_routes routes(params, ctx_server);

    bool                                is_router_server = params.model.path.empty();
    std::optional<server_models_routes> models_routes{};
    if (is_router_server) {
        // setup server instances manager
        try {
            models_routes.emplace(params, argc, argv);
        } catch (const std::exception & e) {
            LOG_ERR("%s: failed to initialize router models: %s\n", __func__, e.what());
            return 1;
        }

        // proxy handlers
        // note: routes.get_health stays the same
        routes.get_metrics                 = models_routes->proxy_get;
        routes.post_props                  = models_routes->proxy_post;
        routes.get_api_show                = models_routes->proxy_get;
        routes.post_completions            = models_routes->proxy_post;
        routes.post_completions_oai        = models_routes->proxy_post;
        routes.post_chat_completions       = models_routes->proxy_post;
        routes.post_responses_oai          = models_routes->proxy_post;
        routes.post_anthropic_messages     = models_routes->proxy_post;
        routes.post_anthropic_count_tokens = models_routes->proxy_post;
        routes.post_infill                 = models_routes->proxy_post;
        routes.post_embeddings             = models_routes->proxy_post;
        routes.post_embeddings_oai         = models_routes->proxy_post;
        routes.post_rerank                 = models_routes->proxy_post;
        routes.post_tokenize               = models_routes->proxy_post;
        routes.post_detokenize             = models_routes->proxy_post;
        routes.post_apply_template         = models_routes->proxy_post;
        routes.get_lora_adapters           = models_routes->proxy_get;
        routes.post_lora_adapters          = models_routes->proxy_post;
        routes.get_slots                   = models_routes->proxy_get;
        routes.post_slots                  = models_routes->proxy_post;

        // custom routes for router
        routes.get_props  = models_routes->get_router_props;
        routes.get_models = models_routes->get_router_models;
        ctx_http.post("/models/load", ex_wrapper(models_routes->post_router_models_load));
        ctx_http.post("/models/unload", ex_wrapper(models_routes->post_router_models_unload));
    }

    ctx_http.get("/health", ex_wrapper(routes.get_health));     // public endpoint (no API key check)
    ctx_http.get("/v1/health", ex_wrapper(routes.get_health));  // public endpoint (no API key check)
    ctx_http.get("/metrics", ex_wrapper(routes.get_metrics));
    ctx_http.get("/props", ex_wrapper(routes.get_props));
    ctx_http.post("/props", ex_wrapper(routes.post_props));
    ctx_http.post("/api/show", ex_wrapper(routes.get_api_show));
    ctx_http.get("/models", ex_wrapper(routes.get_models));     // public endpoint (no API key check)
    ctx_http.get("/v1/models", ex_wrapper(routes.get_models));  // public endpoint (no API key check)
    ctx_http.get("/api/tags",
                 ex_wrapper(routes.get_models));  // ollama specific endpoint. public endpoint (no API key check)
    ctx_http.post("/completion", ex_wrapper(routes.post_completions));  // legacy
    ctx_http.post("/completions", ex_wrapper(routes.post_completions));
    ctx_http.post("/v1/completions", ex_wrapper(routes.post_completions_oai));
    ctx_http.post("/chat/completions", ex_wrapper(routes.post_chat_completions));
    ctx_http.post("/v1/chat/completions", ex_wrapper(routes.post_chat_completions));
    ctx_http.post("/api/chat", ex_wrapper(routes.post_chat_completions));       // ollama specific endpoint
    ctx_http.post("/v1/responses", ex_wrapper(routes.post_responses_oai));
    ctx_http.post("/v1/messages", ex_wrapper(routes.post_anthropic_messages));  // anthropic messages API
    ctx_http.post("/v1/messages/count_tokens",
                  ex_wrapper(routes.post_anthropic_count_tokens));              // anthropic token counting
    ctx_http.post("/infill", ex_wrapper(routes.post_infill));
    ctx_http.post("/embedding", ex_wrapper(routes.post_embeddings));            // legacy
    ctx_http.post("/embeddings", ex_wrapper(routes.post_embeddings));
    ctx_http.post("/v1/embeddings", ex_wrapper(routes.post_embeddings_oai));
    ctx_http.post("/rerank", ex_wrapper(routes.post_rerank));
    ctx_http.post("/reranking", ex_wrapper(routes.post_rerank));
    ctx_http.post("/v1/rerank", ex_wrapper(routes.post_rerank));
    ctx_http.post("/v1/reranking", ex_wrapper(routes.post_rerank));
    ctx_http.post("/tokenize", ex_wrapper(routes.post_tokenize));
    ctx_http.post("/detokenize", ex_wrapper(routes.post_detokenize));
    ctx_http.post("/apply-template", ex_wrapper(routes.post_apply_template));
    // LoRA adapters hotswap
    ctx_http.get("/lora-adapters", ex_wrapper(routes.get_lora_adapters));
    ctx_http.post("/lora-adapters", ex_wrapper(routes.post_lora_adapters));
    // Save & load slots
    ctx_http.get("/slots", ex_wrapper(routes.get_slots));
    ctx_http.post("/slots/:id_slot", ex_wrapper(routes.post_slots));

    //
    // Start the server
    //

    std::function<void()> clean_up;

    if (is_router_server) {
        LOG_INF("%s: starting router server, no model will be loaded in this process\n", __func__);

        clean_up = [&models_routes]() {
            SRV_INF("%s: cleaning up before exit...\n", __func__);
            if (models_routes.has_value()) {
                models_routes->models.unload_all();
            }
            llama_backend_free();
        };

        if (!ctx_http.start()) {
            clean_up();
            LOG_ERR("%s: exiting due to HTTP server error\n", __func__);
            return 1;
        }
        ctx_http.is_ready.store(true);

        shutdown_handler = [&](int) {
            ctx_http.stop();
        };

    } else {
        // setup clean up function, to be called before exit
        clean_up = [&ctx_http, &ctx_server]() {
            SRV_INF("%s: cleaning up before exit...\n", __func__);
            ctx_http.stop();
            ctx_server.terminate();
            llama_backend_free();
        };

        // start the HTTP server before loading the model to be able to serve /health requests
        if (!ctx_http.start()) {
            clean_up();
            LOG_ERR("%s: exiting due to HTTP server error\n", __func__);
            return 1;
        }

        // load the model
        LOG_INF("%s: loading model\n", __func__);

        if (!ctx_server.load_model(params)) {
            clean_up();
            if (ctx_http.thread.joinable()) {
                ctx_http.thread.join();
            }
            LOG_ERR("%s: exiting due to model loading error\n", __func__);
            return 1;
        }

        routes.update_meta(ctx_server);
        ctx_http.is_ready.store(true);

        LOG_INF("%s: model loaded\n", __func__);

        if (tui_mode) {
            run_tui(params, ctx_server);
            clean_up();
            return 0;
        }

        shutdown_handler = [&](int) {
            // this will unblock start_loop()
            ctx_server.terminate();
        };
    }

    // TODO: refactor in common/console
#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
    struct sigaction sigint_action;
    sigint_action.sa_handler = signal_handler;
    sigemptyset(&sigint_action.sa_mask);
    sigint_action.sa_flags = 0;
    sigaction(SIGINT, &sigint_action, NULL);
    sigaction(SIGTERM, &sigint_action, NULL);
#elif defined(_WIN32)
    auto console_ctrl_handler = +[](DWORD ctrl_type) -> BOOL {
        return (ctrl_type == CTRL_C_EVENT) ? (signal_handler(SIGINT), true) : false;
    };
    SetConsoleCtrlHandler(reinterpret_cast<PHANDLER_ROUTINE>(console_ctrl_handler), true);
#endif

    if (is_router_server) {
        LOG_INF("%s: router server is listening on %s\n", __func__, ctx_http.listening_address.c_str());
        LOG_INF("%s: NOTE: router mode is experimental\n", __func__);
        LOG_INF("%s:       it is not recommended to use this mode in untrusted environments\n", __func__);
        if (ctx_http.thread.joinable()) {
            ctx_http.thread.join();  // keep the main thread alive
        }

        // when the HTTP server stops, clean up and exit
        clean_up();
    } else {
        LOG_INF("%s: server is listening on %s\n", __func__, ctx_http.listening_address.c_str());
        LOG_INF("%s: starting the main loop...\n", __func__);

        // optionally, notify router server that this instance is ready
        const char * router_port = std::getenv("LLAMA_SERVER_ROUTER_PORT");
        std::thread  monitor_thread;
        if (router_port != nullptr) {
            monitor_thread = server_models::setup_child_server(shutdown_handler);
        }

        // this call blocks the main thread until queue_tasks.terminate() is called
        ctx_server.start_loop();

        clean_up();
        if (ctx_http.thread.joinable()) {
            ctx_http.thread.join();
        }
        if (monitor_thread.joinable()) {
            monitor_thread.join();
        }

        auto * ll_ctx = ctx_server.get_llama_context();
        if (ll_ctx != nullptr) {
            llama_memory_breakdown_print(ll_ctx);
        }
    }

    return 0;
}
