#include "decrypt.h"
#include "tagger.h"

#include <string>
#include <iostream>

void print_help(char const* const argv0)
{
    std::cout << "Usage: " << fs::path{argv0}.stem() << " file_1 [file_2 ... file_n]" << std::endl;
}

void extract_meta(const Json::Value& meta,
                  std::string& album,
                  std::string& title,
                  std::vector<std::string>& artists)
{
    album = meta["album"].asString();
    title = meta["musicName"].asString();
    for(const auto& ar : meta["artist"])
        artists.emplace_back(ar[0].asString());
}

int main(int argc, char const* const argv[])
{
    if(argc <= 1)
    {
        print_help(argv[0]);
        return 255;
    }

    int success_count{0}, failure_count{0};

    for(auto i = 1; i < argc; ++i) // skip argv[0]
    {
        auto msg_prefix{(std::string{"[file: "} += argv[i]) += "] "};
        fs::path in_path{argv[i]};
        if(!fs::exists(in_path))
        {
            std::cerr << msg_prefix << "not exists" << std::endl;
            ++failure_count;
            break;
        }

        std::ifstream in{in_path.generic_u8string(), std::ios::binary};
        if(!in.good())
        {
            std::cerr << msg_prefix << "cannot open" << std::endl;
            ++failure_count;
            break;
        }

        ncmdump cracker{std::move(in)};
        switch(cracker.failure())
        {
        case ncmdump::failure_t::invalid_ncm_format:
            std::cerr << msg_prefix << "invalid ncm format" << std::endl;
            ++failure_count;
            break;

        case ncmdump::failure_t::invalid_metadata:
            std::cerr << msg_prefix << "invalid metadata" << std::endl;
            ++failure_count;
            break;

        default:
            break;
        }

        fs::path out_path{in_path};
        auto out_ext{std::string{"."} += cracker.metadata()["format"].asString()};
        out_path.replace_extension(out_ext);
        if(fs::exists(out_path))
        {
            std::cerr << msg_prefix << "dumpfile already exists" << std::endl;
            ++failure_count;
            break;
        }
        std::ofstream out(out_path.generic_u8string(), std::ios::binary);
        if(!out.good())
        {
            std::cerr << msg_prefix << "cannot create dumpfile" << std::endl;
            ++failure_count;
            break;
        }

        cracker.dump(out);
        out.close();

        std::string album, title;
        std::vector<std::string> artists;
        extract_meta(cracker.metadata(), album, title, artists);
        if(!write_tag(album, title, artists,
                      cracker.extra_info().img_data.get(), cracker.extra_info().img_len, "image/jpeg",
                      out_path))
        {
            std::cerr << msg_prefix << "cannot write metadata" << std::endl;
            if(!fs::remove(out_path))
                std::cerr << msg_prefix << "cannot clean temp" << std::endl;
            ++failure_count;
            break;
        }

        std::cout << "Success: " << in_path << std::endl;
        ++success_count;
    }

    std::cout << "\nTotal: " << argc - 1 << "\tSuccess: " << success_count << "\tFailure: " << failure_count << std::endl;

    if(success_count == argc - 1)
        return 0;
    else if(failure_count == argc - 1)
    {
        std::cerr << "All failed" << std::endl;
        return 2;
    }
    else
        return 1;
}