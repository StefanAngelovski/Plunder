#include "Html.h"

#include <cstdlib>

namespace {
int collect(htmlq_node* node, void* ctx) {
    static_cast<std::vector<Html::Node>*>(ctx)->push_back(node);
    return 0;
}
int takeFirst(htmlq_node* node, void* ctx) {
    *static_cast<Html::Node*>(ctx) = node;
    return 1;  // stop
}
std::string collapseWhitespace(const char* s, size_t len) {
    std::string out;
    out.reserve(len);
    for (size_t i = 0; i < len; ++i) {
        char c = s[i];
        bool ws = c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '\f' || c == '\v';
        if (ws) { if (!out.empty() && out.back() != ' ') out += ' '; }
        else out += c;
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}
}  // namespace

Html::Html(const std::string& html) : doc_(htmlq_parse(html.data(), html.size())) {}

Html::~Html() { htmlq_free(doc_); }

std::vector<Html::Node> Html::all(const std::string& css, Node scope) const {
    std::vector<Node> out;
    if (doc_) htmlq_select(doc_, scope, css.c_str(), collect, &out);
    return out;
}

Html::Node Html::first(const std::string& css, Node scope) const {
    Node found = nullptr;
    if (doc_) htmlq_select(doc_, scope, css.c_str(), takeFirst, &found);
    return found;
}

std::string Html::text(Node node) {
    if (!node) return "";
    size_t len = 0;
    char* t = htmlq_text(node, &len);
    std::string out = t ? collapseWhitespace(t, len) : "";
    std::free(t);
    return out;
}

std::string Html::attr(Node node, const std::string& name) {
    size_t len = 0;
    const char* v = node ? htmlq_attr(node, name.c_str(), &len) : nullptr;
    return v ? std::string(v, len) : "";
}

std::string Html::tag(Node node) {
    size_t len = 0;
    const char* t = node ? htmlq_tag(node, &len) : nullptr;
    return t ? std::string(t, len) : "";
}

Html::Node Html::parent(Node node) { return node ? htmlq_parent(node) : nullptr; }
