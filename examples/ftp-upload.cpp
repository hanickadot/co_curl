#include <co_curl/co_curl.hpp>
#include <co_curl/format.hpp>
#include <exception>
#include <filesystem>
#include <fstream>

auto load_file(std::filesystem::path path) -> co_curl::task<std::vector<char>> {
	std::cout << "Loading file '" << path << "'...\n";

	auto stream = std::ifstream{path, std::ios::binary};

	if (!stream) {
		throw std::runtime_error("Can't open file");
	}

	stream.unsetf(std::ios::skipws);

	stream.seekg(0, std::ios::end);
	std::streampos size = stream.tellg();
	stream.seekg(0, std::ios::beg);

	// reserve capacity
	std::vector<char> output;
	output.reserve(size);

	// read the data:
	output.insert(output.begin(), std::istream_iterator<char>(stream), std::istream_iterator<char>());

	co_return output;
}

auto upload(std::string name, std::string url, std::string username, std::string password, std::filesystem::path filepath) -> co_curl::task<bool> {
	auto postquote = co_curl::list{};
	postquote.append("RNFR temporary.bin");
	postquote.append("RNTO " + name);

	auto handle = co_curl::easy_handle{};

	std::string output;

	const std::vector<char> filedata = co_await load_file(filepath);

	// handle.verbose();
	handle.url(url + "temporary.bin");
	handle.username(username.c_str());
	handle.password(password.c_str());
	handle.upload();
	handle.postquote(postquote);
	handle.infile_size(filedata.size());

	auto reader = [content = std::span(filedata)](std::span<char> target) mutable -> size_t {
		const auto common_length = std::min(target.size(), content.size());

		const auto source = content.first(common_length);

		std::copy(source.begin(), source.end(), target.begin());

		content = content.subspan(common_length);
		return common_length;
	};

	handle.read_callback(reader);

	std::cout << "Uploading '" << name << "' to '" << url << "'... (size = " << co_curl::data_amount(filedata.size()) << ")\n";

	const co_curl::result r = co_await handle.perform();

	if (r) {
		std::cout << "Upload of '" << name << "' done.\n";
	} else {
		std::cout << "Upload of '" << name << "' failed (" << r.string() << ").\n";
	}

	co_return bool(r);
}

int main(int argc, char ** argv) {
	if (argc < 5) {
		std::cout << "usage: ftp-upload URL USERNAME PASSWORD FILENAME\n";
		return 1;
	}

	co_curl::global_init();

	std::string url = argv[1];
	std::string username = argv[2];
	std::string password = argv[3];
	std::filesystem::path in = argv[4];

	auto content = std::span<const char>();

	if (upload(in.filename().string(), url, username, password, in)) {
		return 0;
	} else {
		return 1;
	}
}