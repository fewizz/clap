#pragma once

#include <cxx_util/encoding/encoding.hpp>
#include <initializer_list>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <map>
#include <variant>
#include "parser.hpp"
#include "cxx_util/mb/string.hpp"

namespace clap {

template<enc::encoding Encoding>
struct basic_braced_clap;

template<enc::encoding Encoding>
struct basic_braced_arg {
    using braced_arg = basic_braced_arg<Encoding>;
    using string = mb::basic_string<Encoding>;
    using string_view = mb::basic_string_view<Encoding>;
    using option_t = std::variant<parser_with_arg<Encoding>, braced_arg>;
    using options_map = std::map<string, option_t, std::less<>>;
    using raw_options = std::initializer_list<std::pair<string_view, option_t>>;

    options_map options;

    basic_braced_arg(){}

    auto& option(string_view name, parser_with_arg<Encoding>&& parser) {
        options.emplace(name, std::move(parser));
        return *this;
    }

    auto& option(string_view name, braced_arg&& braced) {
        options.emplace(name, std::move(braced));
        return *this;
    }

    auto& value(string_view name, auto& value) {
        return option(name, value_parser<Encoding>(value));
    }

    auto& braced(string_view name, raw_options options) {
        braced_arg arg;
        for(auto& a : options) { arg.options.emplace(a.first, std::move(a.second)); }
        return option(name, std::move(arg));
    }
};

template<enc::encoding Encoding>
struct basic_braced_clap {
    using braced_arg = basic_braced_arg<Encoding>;

    template<enc::encoding Encoding0>
    using character = mb::character<Encoding0>;
    template<enc::encoding Encoding0>
    using character_view = mb::character_view<Encoding0>;
    
    template<enc::encoding Encoding0>
    using string = mb::basic_string<Encoding0>;
    template<enc::encoding Encoding0>
    using string_view = mb::basic_string_view<Encoding0>;

    using option_t = typename braced_arg::option_t;
    using options_map = typename braced_arg::options_map;
    using raw_options = typename std::initializer_list<std::pair<string_view<Encoding>, option_t>>;
    using string_index_type = typename string<Encoding>::size_type;

    auto& option(string_view<Encoding> name, parser_with_arg<Encoding>&& parser) {
        root.option(name, std::move(parser));
        return *this;
    }

    auto& option(string_view<Encoding> name, braced_arg&& braced) {
        root.option(name, std::move(braced));
        return *this;
    }

    auto& braced(string_view<Encoding> name, raw_options options) {
        root.braced(name, options);
        return * this;
    }

    /*template<enc::encoding Encoding0>
    void parse(const std::ranges::range auto& r) {
        parse(std::ranges::begin(r), std::ranges::end(r));
    }*/

    template<enc::encoding Encoding0, class InputIterator>
    void parse(InputIterator begin, InputIterator end) {
        string_index_type i = 0;
        parse_option<Encoding0>(begin, end, root.options, i);
        if(begin != end) throw std::runtime_error{"unexpected '}'"};
    }

protected:
    braced_arg root;

    template<enc::encoding Encoding0, class InputIterator>
    static inline void
    parse_option(InputIterator& begin, InputIterator end, options_map& options, string_index_type& beginning) {
        auto s = [&]() { return string_view<Encoding0>{*begin}.substr(beginning); };
        auto is_end = [&](){ return begin == end; };

        auto next_word = [&]() -> bool {
            ++begin;
            if(is_end()) return false;
            beginning = 0;
            return true;
        };

        auto skip = [&](string_index_type chars) {
            if(chars >= s().size())
                throw std::runtime_error {
                    "skipped too many characters (" +
                    std::to_string(chars) +
                    ") for string '"
                    +s().template to_string<enc::utf8>().template to_string<char>()
                    +"'"
                };
            else beginning += chars;
        };

        auto next_char = [&]() -> bool {
            if(s().size() == 1) return next_word();
            else if(s().size() > 1) {
                skip(1);
                return true;
            }
            else throw std::runtime_error{"string size is zero"};
        };

        while(begin != end) {
            if(s().front() == '}') return;

            string_index_type closest_index = string<Encoding0>::npos;
            
            for(string_index_type index = 0; index < s().size(); index++)
                if(s()[index] == '=' || s()[index] == '{') {
                    closest_index = index;
                    break;
                }

            // option name, before '='
            string_view<Encoding0> name = s().substr(0, closest_index);
            if(name.empty()) throw std::runtime_error { "option name is empty" };

            auto name_to_option = options.find(name);
            if(name_to_option == options.end())
                throw std::runtime_error{
                    "can't find option '"
                    +name.template to_string<enc::utf8>().template to_string<char>()
                    +"'"
                };
            auto& option = name_to_option->second;

            if(closest_index == string<Encoding>::npos) {
                if(!next_word())
                    throw std::runtime_error{
                        "there's no value for option '"
                        +name.template to_string<enc::utf8>().template to_string<char>()
                        +"'"
                    };
            }
            else skip(closest_index);

            // '=' or '{'
            auto ch = s().front();

            // skip '=' or '{'
            if(!next_char())
                throw std::runtime_error{
                    "unexpected end after '"
                    +name.template to_string<enc::utf8>().template to_string<char>()
                    +" "+ch.template to_string<enc::utf8>().template to_string<char>()+"'"
                };

            if(ch == '{') {
                parse_option<Encoding0>(begin, end, std::get<braced_arg>(option).options, beginning);
                if(is_end() || s().front() != '}')
                    throw std::runtime_error{
                        "unexpected end of branced option '"
                        +name.template to_string<enc::utf8>().template to_string<char>()
                        +"'"
                    };
                if(!next_char()) return;
            }
            else if(ch == '=') {
                auto closing_index = s().find('}');
                auto arg = s().substr(0, closing_index);
                if(arg.empty()) throw std::runtime_error{
                    "value of option '"
                    +name.template to_string<enc::utf8>().template to_string<char>()
                    +"' is empty"
                };
                std::get<parser_with_arg<Encoding>>(option) (arg);
 
                // we have '}' here. skipping and returning to parent
                if(closing_index != string<Encoding>::npos) {
                    skip(closing_index);
                    return;
                }
 
                // we parsed whole word as option argument, grab next
                if(!next_word()) return;
                // it could be at the beginning of next word
            }
            else throw std::runtime_error {
                "undefined character '"
                +ch.template to_string<enc::utf8>().template to_string<char>()
                +"' after option name '"
                +name.template to_string<enc::utf8>().template to_string<char>()
                +"'"
            };
        }
    }

};

}