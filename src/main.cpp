#include <iostream>
#include <sstream>
#include <functional>
#include <optional>
#include <algorithm>
#include <iterator>
#include <vector>
#include <variant>
#include <tuple>
#include <boost/core/demangle.hpp>
#include <nlohmann/json.hpp>
#include "addressbook.pb.h"

struct StrmEnd {};

template <typename T>
using Expected = std::variant<StrmEnd, T>;

template <typename T>
bool isBad(T&& t) { return std::holds_alternative<StrmEnd>(t); }

template <typename T>
auto getGood(T&& t) { return std::get<1>(t); }

std::ostream& operator<<(std::ostream& os, const std::vector<uint8_t>& data)
{
    os << "[ ";
    for (auto x : data) {
        os << int(x) << ' ';
    }
    os << ']';
    return os;
}

template <typename T>
struct Tagged_t {
    T  data;

    Tagged_t(T&& t) : data(t) {}
    operator T& () { return data; }
};


class VarIntIterator {
public:
    using iterator_category = std::input_iterator_tag;
    using value_type = uint8_t;
    using difference_type = std::ptrdiff_t;
    using pointer = const uint8_t*;
    // using reference = uint64_t;

    explicit VarIntIterator(const uint8_t* start, size_t len)
    {
        initial = current = start;
        end = current + len;
        atEnd = false;
        evaluate();
    }

    explicit VarIntIterator(const std::string& s) : VarIntIterator((uint8_t*)(s.data()), s.size())
    {
    }

    explicit VarIntIterator(const std::vector<uint8_t>& s) : VarIntIterator(s.data(), s.size())
    {
    }

    Expected<uint64_t> operator*() const {
        if (atEnd) return StrmEnd{};
        return accum;
    }

    VarIntIterator& operator++() {
        evaluate();
        return *this;
    }

    Expected<std::vector<uint8_t>> operator()(size_t len)
    {
        using namespace std;
        if (current + len > end) return StrmEnd{};
        auto ret = std::vector<uint8_t>(current, current + len);
        current += len;
        // cout << "jumping current to " << dec << (current - initial) << endl;
        evaluate();
        return ret;
    }

    size_t offset() const
    {
        return current - initial;
    }

private:
    pointer initial;
    pointer current;
    pointer end;
    uint64_t accum;
    size_t shift;
    bool atEnd;

    void evaluate()
    {
        using namespace std;

        if (atEnd) return;
        accum = 0;
        shift = 0;
        while (current < end) {
            auto value = *current;
            ++current;
            accum |= uint64_t(value & 0x7f) << shift;
            shift += 7;
            if (!(value & 0x80)) return;
        }
        atEnd = true;
    }
};

template <typename T>
T packInto(const std::vector<uint8_t>& data)
{
    auto len = sizeof(T);
    T ret;
    for (auto i = 0; i < len; i++) {
        ret <<= 8;
        ret |= data[i];
    }
    return ret;
}

using VarInt_t = Tagged_t<size_t>;

using FieldVar = std::variant<VarInt_t, uint32_t, uint64_t, std::vector<uint8_t>>;

std::ostream& operator<<(std::ostream& os, const FieldVar& data)
{
    using namespace std;
    if (holds_alternative<VarInt_t>(data)) {
        size_t s = get<VarInt_t>(data).data;
        os << "size_t(" << s << ')';
    }
    else if (holds_alternative<uint32_t>(data))
        os << "uint32_t(" << get<uint32_t>(data) << ')';
    else if (holds_alternative<uint64_t>(data))
        os << "uint64_t(" << get<uint64_t>(data) << ')';
    else if (holds_alternative<vector<uint8_t>>(data)) {
        os << "vector<uint8_t>(" << get<vector<uint8_t>>(data) << ')';
    }
    return os;
}

Expected<std::tuple<size_t, FieldVar>>
    takeField(VarIntIterator& it)
{
    using namespace std;
    auto vtag = *it;
    if (isBad(vtag)) return StrmEnd{};
    auto tag = getGood(vtag);
    size_t fieldNumber = tag >> 3;
    tag &= 7;
    switch (tag) {
        case 0: {
            ++it;
            auto value = *it;
            if (isBad(value)) return StrmEnd{};
            ++it;
            return make_tuple(fieldNumber, VarInt_t(getGood(value)));
        }
        case 1: {
            auto value = it(sizeof(uint64_t));
            if (isBad(value)) return StrmEnd{};
            return make_tuple(fieldNumber, packInto<uint64_t>(getGood(value)));
        }
        case 5: {
            auto value = it(sizeof(uint32_t));
            if (isBad(value)) return StrmEnd{};
            return make_tuple(fieldNumber, packInto<uint32_t>(getGood(value)));
        }
        case 2: {
            ++it;
            auto len = *it;
            if (isBad(len)) return StrmEnd{};
            auto value = it(getGood(len));
            if (isBad(value)) return StrmEnd{};
            return make_tuple(fieldNumber, getGood(value));
        }
        default:
            cout << "tag incorrect" << endl;
            return StrmEnd{};
    }
}

int main(int argc, char *argv[])
{
    using namespace std;

    auto attr = new tutorial::Attributes();
    attr->set_name("name");
    attr->set_id(1000000);
    attr->set_email("email");

    auto msg = new tutorial::Message();
    msg->set_allocated_attributes(attr);
    msg->set_payload("data");

    cout << msg->DebugString() << endl;
    auto x = msg->SerializeAsString();
    // for (auto i = 0; i < x.size(); i++) cout << dec << i << ": " << hex << int(x[i]) << endl;

    auto it = VarIntIterator(x);

    {
        auto v = takeField(it);
        if (isBad(v)) {
            cout << "failed to get field" << endl;
            exit(-1);
        }
        auto [ fieldNumber, data] = getGood(v);
        if (holds_alternative<vector<uint8_t>>(data)) {
            cout << fieldNumber << ": " << data << endl;
            auto data2 = get<vector<uint8_t>>(data);
            auto it2 = VarIntIterator(data2);
            while (true) {
                auto v2 = takeField(it2);
                if (isBad(v2)) break;
                auto [ fieldNumber3, data3] = getGood(v2);
                cout << fieldNumber3 << ": " << data3 << endl;
            }
        }
        else {
            cout << fieldNumber << ": " << data << endl;
        }
    }
    {
        auto v = takeField(it);
        if (isBad(v)) {
            cout << "failed to get field" << endl;
            exit(-1);
        }
        auto [ fieldNumber, data] = getGood(v);
        cout << fieldNumber << ": " << data << endl;
    }
}
