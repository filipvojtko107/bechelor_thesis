#ifndef __HTTP_1_1_HPP__
#define __HTTP_1_1_HPP__
#include "Http1_0.hpp"
#include <vector>


class Http1_1 : public Http1_0
{
    public:
        Http1_1(const std::shared_ptr<TcpServer>& tcp_server, std::shared_ptr<TcpServer::Connection>& connection);
        ~Http1_1() = default;

        void reset() override;
    
    protected:
        void handleConnection() override;

        bool requestGetMethod() override;
        bool requestHeadMethod() override;
        bool requestPostMethod() override;
        bool requestPutMethod() override;
        bool requestDeleteMethod() override;
        bool requestOptionsMethod();
        // void requestPatchMethod();

        int headersRange(const bool parse_ranges = false);
        int headersIfRange();
        int headersIfMatch();
        int headersIfNoneMatch(const bool rsrc_exists_check = false);
        int headersIfUnmodifiedSince();
        int headersExpect();

        bool checkResourceConstraints() override;
        bool sendResponseRanges();

    private:
        enum class RangeType
        {
            START_END,
            START_INF,
            SUFFIX_LENGTH,
            INVALID
        };

        struct Range
        {
            uint64_t start = 0;
            uint64_t end = 0;
            RangeType type = RangeType::INVALID;
        };

    private:
        std::vector<Range> ranges_;
};

#endif