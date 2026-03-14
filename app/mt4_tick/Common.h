#pragma once

static std::vector<std::string> StringSplite(const std::string& src, std::string e) {
	if (src.length() == 0) return std::vector<std::string>();     // 空
	if (e.length() == 0) return std::vector<std::string>({ src });// 源
	std::string source = src + e; // 尾部添加一个分隔符
	std::vector<std::string> v_res;
	size_t s = 0;
	size_t b = source.find(e, s);
	while (std::string::npos != b) {
		std::string sss = source.substr(s, b - s);  // 防止尾部为 e， 添加空值进 vector
		if (sss.length() > 0)
			v_res.emplace_back(source.substr(s, b - s));
		s = b + e.length();
		b = source.find(e, s);
	}
	return v_res;
};