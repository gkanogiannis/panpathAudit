#include "panpath/input.hpp"
#include "panpath/sequence.hpp"

#include <array>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <zlib.h>

namespace panpath {

const char* category_name(DiagnosticCategory category) {
    switch (category) {
        case DiagnosticCategory::source: return "source";
        case DiagnosticCategory::gfa: return "gfa";
        case DiagnosticCategory::correspondence: return "correspondence";
        case DiagnosticCategory::operational: return "operational";
        case DiagnosticCategory::internal: return "internal";
    }
    return "internal";
}

std::string SourceSpec::identifier(const std::string& raw) const {
    if (mode == SourceMode::pansn) return sample + "#" + haplotype + "#" + raw;
    if (mode == SourceMode::prefix) return prefix + raw;
    return raw;
}

struct LineReader::Impl {
    Compression compression = Compression::plain;
    std::ifstream plain;
    gzFile gzip = nullptr;
};

LineReader::LineReader(const std::filesystem::path& path) : impl_(std::make_unique<Impl>()) {
    std::ifstream probe(path, std::ios::binary);
    if (!probe) throw std::runtime_error("cannot open input");
    std::array<unsigned char, 2> magic{};
    probe.read(reinterpret_cast<char*>(magic.data()), 2);
    impl_->compression = probe.gcount() == 2 && magic[0] == 0x1f && magic[1] == 0x8b
        ? Compression::gzip : Compression::plain;
    if (impl_->compression == Compression::gzip) {
#ifdef _WIN32
        impl_->gzip = gzopen_w(path.c_str(), "rb");
#else
        impl_->gzip = gzopen(path.c_str(), "rb");
#endif
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
            if (!line.empty()) return true;
            int code = Z_OK;
            const char* message = gzerror(impl_->gzip, &code);
            if (code != Z_OK && code != Z_STREAM_END) throw std::runtime_error(message);
            return false;
        }
        line += value;
        if (!line.empty() && line.back() == '\n') { line.pop_back(); return true; }
    }
}

void stream_fasta(const std::filesystem::path& path, const FastaVisitor& visitor,
                  const ProgressCallback& progress) {
    LineReader reader(path);
    std::string line, name, sequence;
    std::size_t lines = 0;
    bool found = false;
    while (reader.read(line)) {
        if (progress && (++lines & 1023U) == 0) progress();
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty() && line.front() == '>') {
            if (found) { visitor(name, sequence); sequence.clear(); }
            std::istringstream header(line.substr(1));
            if (!(header >> name)) throw std::runtime_error("invalid FASTA header");
            found = true;
        } else if (found) {
            sequence += line;
        } else if (!line.empty()) {
            throw std::runtime_error("sequence precedes first FASTA header");
        }
    }
    if (!found) throw std::runtime_error("empty FASTA");
    visitor(name, sequence);
    if (progress) progress();
}

SourceIndex index_sources(const std::vector<SourceSpec>& sources,
                          const ProgressCallback& progress) {
    SourceIndex result;
    result.compressions.reserve(sources.size());
    for (const auto& source : sources) {
        try {
            LineReader reader(source.path);
            result.compressions.push_back(reader.compression() == Compression::gzip ? "gzip" : "plain");
        } catch (const std::exception& error) {
            result.compressions.emplace_back();
            result.diagnostics.push_back({DiagnosticCategory::source, "FASTA_READ",
                error.what(), source.path});
            continue;
        }
        try {
            stream_fasta(source.path, [&](const std::string& raw, const std::string& sequence) {
                const auto identifier = source.identifier(raw);
                for (std::size_t i = 0; i < sequence.size(); ++i) {
                    if (!is_iupac(sequence[i])) {
                        Diagnostic diagnostic{DiagnosticCategory::source,
                            "INVALID_SOURCE_NUCLEOTIDE", "invalid source nucleotide", source.path};
                        diagnostic.identifier = identifier;
                        diagnostic.position = i + 1;
                        diagnostic.symbol = sequence.substr(i, 1);
                        result.diagnostics.push_back(std::move(diagnostic));
                        break;
                    }
                }
                if (!result.lengths.emplace(identifier, sequence.size()).second) {
                    Diagnostic diagnostic{DiagnosticCategory::source,
                        "DUPLICATE_SEQUENCE_IDENTIFIER", "duplicate sequence identifier", source.path};
                    diagnostic.identifier = identifier;
                    result.diagnostics.push_back(std::move(diagnostic));
                }
            }, progress);
        } catch (const std::exception& error) {
            result.diagnostics.push_back({DiagnosticCategory::source, "INVALID_FASTA",
                error.what(), source.path});
        }
    }
    return result;
}

}
