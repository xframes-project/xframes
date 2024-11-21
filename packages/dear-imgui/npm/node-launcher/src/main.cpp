#include <iostream>
#include <node.h>  // Make sure you include Node.js headers
#include <v8.h>
#include <uv.h>
#include <libplatform/libplatform.h>

// std::vector<std::string> args = {"node", "app.js"};
std::vector<std::string> args;
std::vector<std::string> exec_args;

// Entry point for the C++ application
int main(int argc, char* argv[]) {

    auto platform = v8::platform::NewDefaultPlatform();

    uv_loop_t* loop = uv_default_loop();

    v8::V8::InitializeICU();
    v8::V8::InitializeExternalStartupData(".");
    v8::V8::InitializePlatform(platform.get());
    v8::V8::Initialize();

    // Create a new V8 isolate
    v8::Isolate::CreateParams create_params;
    create_params.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
    auto isolate = v8::Isolate::New(create_params);

    // Create isolate data
    v8::Locker locker(isolate);
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);

    // Create a new context
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(context);

    auto isolate_data = node::CreateIsolateData(isolate, loop);

    // Create the Node.js environment
    node::Environment* env = node::CreateEnvironment(
        isolate_data,
        context,
        args,
        exec_args
    );

    // Step 5: Run the JavaScript code
    const char* script = R"(
            const fs = require('fs');
            console.log('Hello from Node.js embedded in C++!');
        )";

    // Compile and execute the script
    v8::Local<v8::String> source =
        v8::String::NewFromUtf8(isolate, script, v8::NewStringType::kNormal).ToLocalChecked();

    v8::Local<v8::Script> compiled_script;
    if (!v8::Script::Compile(context, source).ToLocal(&compiled_script)) {
        std::cerr << "Failed to compile script" << std::endl;
    } else {
        compiled_script->Run(context).ToLocalChecked();
    }

    node::EmitBeforeExit(env);
    uv_run(loop, UV_RUN_DEFAULT);
    node::EmitExit(env);
    node::Stop(env);

    // Cleanup
    node::FreeEnvironment(env);
    node::FreeIsolateData(isolate_data);

    uv_loop_close(loop);

    return 0;
}