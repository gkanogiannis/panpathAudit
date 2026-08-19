#include "panpath/input.hpp"
#include "panpath/sequence.hpp"

#include <array>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <zlib.h>

namespace panpath {
struct LineReader::Impl {
    Compression compression = Compression::plain;
    std::ifstream plain;
    gzFile gzip = nullptr;
};

LineReader::LineReader(const std::filesystem::path& path) : impl_(std::make_unique<Impl>()) {
    std::ifstream probe(path, std::ios::binary);
    if (!probe) throw std::runtime_error("No such file or directory (os error 2)");
    std::array<unsigned char, 2> magic{};
    probe.read(reinterpret_cast<char*>(magic.data()), 2);
    impl_->compression = probe.gcount() == 2 && magic[0] == 0x1f && magic[1] == 0x8b
        ? Compression::gzip : Compression::plain;
    if (impl_->compression == Compression::gzip) {
        impl_->gzip = gzopen(path.string().c_str(), "rb");
        if (!impl_->gzip) throw std::runtime_error("cannot open gzip input");
    } else {
        impl_->plain.open(path, std::ios::binary);
        if (!impl_->plain) throw std::runtime_error("cannot open input");
    }
}

LineReader::~LineReader() { if (impl_ && impl_->gzip) gzclose(impl_->gzip); }
LineReader::LineReader(LineReader&&) noexcept = default;
LineReader& LineReader::operator=(LineReader&&) noexcept = default;
Compression LineReader::compression() const { return impl_->compression; }

bool LineReader::read(std::string& line) {
    line.clear();
    if (impl_->compression == Compression::plain) return static_cast<bool>(std::getline(impl_->plain, line));
    std::array<char, 64 * 1024> buffer{};
    while (true) {
        char* value = gzgets(impl_->gzip, buffer.data(), static_cast<int>(buffer.size()));
        if (!value) {
            if (line.empty()) {
                int code = Z_OK; const char* message = gzerror(impl_->gzip, &code);
                if (code != Z_OK && code != Z_STREAM_END) throw std::runtime_error(message);
                return false;
            }
            return true;
        }
        line += value;
        if (!line.empty() && line.back() == '\n') { line.pop_back(); return true; }
    }
}

void stream_fasta(const std::filesystem::path& path, const FastaVisitor& visitor) {
    LineReader reader(path);
    std::string line, name, sequence;
    bool found = false;
    while (reader.read(line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty() && line.front() == '>') {
            if (found) { visitor(name, sequence); sequence.clear(); }
            std::istringstream header(line.substr(1));
            if (!(header >> name)) throw std::runtime_error("invalid FASTA header");
            found = true;
        } else sequence += line;
    }
    if (!found) throw std::runtime_error("empty FASTA");
    visitor(name, sequence);
}

SourceIndex index_sources(const std::vector<SourceSpec>& sources) {
    SourceIndex result;
    for (const auto& source : sources) {
        try {
            LineReader reader(source.path);
            result.compressions.push_back(reader.compression() == Compression::gzip ? "gzip" : "plain");
        } catch (const std::exception& error) {
            result.errors.push_back("FASTA_READ path=" + source.path.string() + " error=" + error.what());
            continue;
        }
        try {
            stream_fasta(source.path, [&](const std::string& raw, const std::string& sequence) {
                const auto identifier = source.identifier(raw);
                for (std::size_t i = 0; i < sequence.size(); ++i) if (!is_iupac(sequence[i])) {
                    result.errors.push_back("INVALID_SOURCE_NUCLEOTIDE path=" + source.path.string() +
                        " identifier=" + identifier + " position=" + std::to_string(i + 1) +
                        " symbol=" + sequence.substr(i, 1));
                    break;
                }
                if (!result.lengths.emplace(identifier, sequence.size()).second)
                    result.errors.push_back("DUPLICATE_SEQUENCE_IDENTIFIER identifier=" + identifier);
            });
        } catch (const std::exception& error) {
            result.errors.push_back("INVALID_FASTA path=" + source.path.string() + " error=" + error.what());
        }
    }
    return result;
}
}
