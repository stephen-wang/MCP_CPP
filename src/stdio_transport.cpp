#include <mcp/client/stdio_transport.hpp>
#include <mcp/server/stdio_transport.hpp>

#include <mcp/detail/stdio_transport_base.hpp>

#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace mcp::detail
{

    class StdioTransportCore
    {
    public:
        using MessageHandler = mcp::ITransport::MessageHandler;
        using ErrorHandler = mcp::ITransport::ErrorHandler;
        using CloseHandler = mcp::ITransport::CloseHandler;

        StdioTransportCore(std::istream &input, std::ostream &output)
            : state_(std::make_shared<State>(input, output))
        {
        }

        void start()
        {
            {
                std::lock_guard<std::mutex> lock(state_->lifecycle_mutex);
                if (state_->active)
                {
                    return;
                }

                if (state_->ever_started)
                {
                    throw std::logic_error("stdio transport cannot be restarted after close");
                }

                state_->active = true;
                state_->ever_started = true;
                state_->close_notified = false;
            }

            std::thread([state = state_]()
                        { read_loop(std::move(state)); })
                .detach();
        }

        void close()
        {
            CloseHandler close_handler;

            {
                std::lock_guard<std::mutex> lock(state_->lifecycle_mutex);
                if (state_->close_notified)
                {
                    return;
                }

                state_->active = false;
                state_->close_notified = true;
            }

            {
                std::lock_guard<std::mutex> lock(state_->handler_mutex);
                close_handler = state_->close_handler;
            }

            if (close_handler)
            {
                close_handler();
            }
        }

        void send(const std::string &message)
        {
            {
                std::lock_guard<std::mutex> lock(state_->lifecycle_mutex);
                if (!state_->active)
                {
                    throw std::logic_error("stdio transport is not active");
                }
            }

            {
                std::lock_guard<std::mutex> lock(state_->output_mutex);
                (*state_->output) << message << '\n';
                state_->output->flush();
            }

            if (!(*state_->output))
            {
                emit_error("stdio transport failed to write outbound message");
                throw std::runtime_error("stdio transport failed to write outbound message");
            }
        }

        void set_message_handler(MessageHandler handler)
        {
            std::lock_guard<std::mutex> lock(state_->handler_mutex);
            state_->message_handler = std::move(handler);
        }

        void set_error_handler(ErrorHandler handler)
        {
            std::lock_guard<std::mutex> lock(state_->handler_mutex);
            state_->error_handler = std::move(handler);
        }

        void set_close_handler(CloseHandler handler)
        {
            std::lock_guard<std::mutex> lock(state_->handler_mutex);
            state_->close_handler = std::move(handler);
        }

    private:
        struct State
        {
            State(std::istream &input_stream, std::ostream &output_stream)
                : input(&input_stream), output(&output_stream)
            {
            }

            std::istream *input;
            std::ostream *output;
            std::mutex handler_mutex;
            std::mutex lifecycle_mutex;
            std::mutex output_mutex;
            MessageHandler message_handler;
            ErrorHandler error_handler;
            CloseHandler close_handler;
            bool active = false;
            bool ever_started = false;
            bool close_notified = false;
        };

        static void read_loop(std::shared_ptr<State> state)
        {
            for (;;)
            {
                std::string line;
                if (!std::getline(*state->input, line))
                {
                    if (state->input->bad())
                    {
                        emit_error(state, "stdio transport failed to read inbound message");
                    }

                    return;
                }

                MessageHandler message_handler;
                {
                    std::lock_guard<std::mutex> lifecycle_lock(state->lifecycle_mutex);
                    if (!state->active)
                    {
                        return;
                    }
                }

                {
                    std::lock_guard<std::mutex> handler_lock(state->handler_mutex);
                    message_handler = state->message_handler;
                }

                if (message_handler)
                {
                    message_handler(line);
                }
            }
        }

        static void emit_error(const std::shared_ptr<State> &state, const std::string &message)
        {
            ErrorHandler error_handler;
            {
                std::lock_guard<std::mutex> lock(state->handler_mutex);
                error_handler = state->error_handler;
            }

            if (error_handler)
            {
                error_handler(message);
            }
        }

        void emit_error(const std::string &message)
        {
            emit_error(state_, message);
        }

        std::shared_ptr<State> state_;
    };

    template <typename TTransportInterface>
    class StdioTransportBase<TTransportInterface>::Impl
    {
    public:
        Impl(std::istream &input, std::ostream &output)
            : core_(input, output)
        {
        }

        void start()
        {
            core_.start();
        }

        void close()
        {
            core_.close();
        }

        void send(const std::string &message)
        {
            core_.send(message);
        }

        void set_message_handler(typename StdioTransportBase<TTransportInterface>::MessageHandler handler)
        {
            core_.set_message_handler(std::move(handler));
        }

        void set_error_handler(typename StdioTransportBase<TTransportInterface>::ErrorHandler handler)
        {
            core_.set_error_handler(std::move(handler));
        }

        void set_close_handler(typename StdioTransportBase<TTransportInterface>::CloseHandler handler)
        {
            core_.set_close_handler(std::move(handler));
        }

    private:
        mcp::detail::StdioTransportCore core_;
    };

    template <typename TTransportInterface>
    StdioTransportBase<TTransportInterface>::StdioTransportBase()
        : impl_(std::make_unique<Impl>(std::cin, std::cout))
    {
    }

    template <typename TTransportInterface>
    StdioTransportBase<TTransportInterface>::StdioTransportBase(std::istream &input, std::ostream &output)
        : impl_(std::make_unique<Impl>(input, output))
    {
    }

    template <typename TTransportInterface>
    StdioTransportBase<TTransportInterface>::~StdioTransportBase() = default;

    template <typename TTransportInterface>
    StdioTransportBase<TTransportInterface>::StdioTransportBase(StdioTransportBase &&) noexcept = default;

    template <typename TTransportInterface>
    StdioTransportBase<TTransportInterface> &StdioTransportBase<TTransportInterface>::operator=(StdioTransportBase &&) noexcept = default;

    template <typename TTransportInterface>
    void StdioTransportBase<TTransportInterface>::start()
    {
        impl_->start();
    }

    template <typename TTransportInterface>
    void StdioTransportBase<TTransportInterface>::close()
    {
        impl_->close();
    }

    template <typename TTransportInterface>
    void StdioTransportBase<TTransportInterface>::send(const std::string &message)
    {
        impl_->send(message);
    }

    template <typename TTransportInterface>
    void StdioTransportBase<TTransportInterface>::set_message_handler(typename StdioTransportBase<TTransportInterface>::MessageHandler handler)
    {
        impl_->set_message_handler(std::move(handler));
    }

    template <typename TTransportInterface>
    void StdioTransportBase<TTransportInterface>::set_error_handler(typename StdioTransportBase<TTransportInterface>::ErrorHandler handler)
    {
        impl_->set_error_handler(std::move(handler));
    }

    template <typename TTransportInterface>
    void StdioTransportBase<TTransportInterface>::set_close_handler(typename StdioTransportBase<TTransportInterface>::CloseHandler handler)
    {
        impl_->set_close_handler(std::move(handler));
    }

    template class StdioTransportBase<mcp::client::IClientTransport>;
    template class StdioTransportBase<mcp::server::IServerTransport>;

} // namespace mcp::detail

namespace mcp::client
{

    void StdioTransport::connect()
    {
        start();
    }

} // namespace mcp::client

namespace mcp::server
{

    void StdioTransport::listen()
    {
        start();
    }

} // namespace mcp::server
