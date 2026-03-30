#pragma once

#include <iosfwd>
#include <memory>
#include <string>

namespace mcp::detail
{

    template <typename TTransportInterface>
    class StdioTransportBase : public TTransportInterface
    {
    public:
        using MessageHandler = typename TTransportInterface::MessageHandler;
        using ErrorHandler = typename TTransportInterface::ErrorHandler;
        using CloseHandler = typename TTransportInterface::CloseHandler;

        StdioTransportBase();
        StdioTransportBase(std::istream &input, std::ostream &output);
        ~StdioTransportBase() override;

        StdioTransportBase(const StdioTransportBase &) = delete;
        StdioTransportBase &operator=(const StdioTransportBase &) = delete;
        StdioTransportBase(StdioTransportBase &&) noexcept;
        StdioTransportBase &operator=(StdioTransportBase &&) noexcept;

        void close() override;
        void send(const std::string &message) override;
        void set_message_handler(MessageHandler handler) override;
        void set_error_handler(ErrorHandler handler) override;
        void set_close_handler(CloseHandler handler) override;

    protected:
        void start();

    private:
        class Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace mcp::detail