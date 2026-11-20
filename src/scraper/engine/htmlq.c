/*
 * htmlq implementation. Compiles lexbor's single-file build into this one
 * translation unit, so nothing else needs lexbor's headers.
 */
#include "htmlq.h"

#include "../../../third_party/lexbor/lexbor.c"

#include <stdlib.h>
#include <string.h>

#define HTMLQ_CACHE_SIZE 48

struct htmlq_doc {
    lxb_html_document_t* doc;
    lxb_css_parser_t* parser;
    lxb_selectors_t* selectors;
    /* Compiled selectors, by selector text. All lists parsed by one CSS
     * parser share one memory pool, which is freed once in htmlq_free(). */
    struct { char* css; lxb_css_selector_list_t* list; } cache[HTMLQ_CACHE_SIZE];
    int cached;
    lxb_css_memory_t* pool;
};

static lxb_dom_node_t* as_node(htmlq_node* n) { return (lxb_dom_node_t*)n; }

htmlq_doc* htmlq_parse(const char* html, size_t len) {
    htmlq_doc* d = (htmlq_doc*)calloc(1, sizeof(htmlq_doc));
    if (!d) return NULL;
    d->doc = lxb_html_document_create();
    d->parser = lxb_css_parser_create();
    d->selectors = lxb_selectors_create();
    if (!d->doc || !d->parser || !d->selectors ||
        lxb_html_document_parse(d->doc, (const lxb_char_t*)html, len) != LXB_STATUS_OK ||
        lxb_css_parser_init(d->parser, NULL) != LXB_STATUS_OK ||
        lxb_selectors_init(d->selectors) != LXB_STATUS_OK) {
        htmlq_free(d);
        return NULL;
    }
    return d;
}

void htmlq_free(htmlq_doc* d) {
    int i;
    if (!d) return;
    for (i = 0; i < d->cached; ++i) free(d->cache[i].css);
    if (d->selectors) lxb_selectors_destroy(d->selectors, true);
    if (d->pool) lxb_css_memory_destroy(d->pool, true);
    if (d->parser) lxb_css_parser_destroy(d->parser, true);
    if (d->doc) lxb_html_document_destroy(d->doc);
    free(d);
}

htmlq_node* htmlq_root(htmlq_doc* d) {
    return d ? (htmlq_node*)lxb_dom_interface_node(d->doc) : NULL;
}

static lxb_css_selector_list_t* compiled(htmlq_doc* d, const char* css) {
    int i;
    lxb_css_selector_list_t* list;
    for (i = 0; i < d->cached; ++i)
        if (strcmp(d->cache[i].css, css) == 0) return d->cache[i].list;
    list = lxb_css_selectors_parse(d->parser, (const lxb_char_t*)css, strlen(css));
    if (d->parser->status != LXB_STATUS_OK) list = NULL;
    if (list && !d->pool) d->pool = list->memory;
    if (d->cached < HTMLQ_CACHE_SIZE) {
        d->cache[d->cached].css = strdup(css);
        d->cache[d->cached].list = list;
        if (d->cache[d->cached].css) d->cached++;
    }
    return list;
}

struct select_ctx {
    int (*cb)(htmlq_node*, void*);
    void* ctx;
    int count;
};

static lxb_status_t on_match(lxb_dom_node_t* node, lxb_css_selector_specificity_t spec, void* ctx) {
    struct select_ctx* s = (struct select_ctx*)ctx;
    (void)spec;
    s->count++;
    return s->cb((htmlq_node*)node, s->ctx) ? LXB_STATUS_STOP : LXB_STATUS_OK;
}

int htmlq_select(htmlq_doc* d, htmlq_node* scope, const char* css,
                 int (*cb)(htmlq_node*, void*), void* ctx) {
    struct select_ctx s;
    lxb_css_selector_list_t* list;
    if (!d || !css || !cb) return -1;
    list = compiled(d, css);
    if (!list) return -1;
    s.cb = cb;
    s.ctx = ctx;
    s.count = 0;
    lxb_selectors_find(d->selectors, scope ? as_node(scope) : lxb_dom_interface_node(d->doc), list, on_match, &s);
    return s.count;
}

char* htmlq_text(htmlq_node* n, size_t* len) {
    size_t l = 0;
    lxb_char_t* t;
    char* out;
    if (len) *len = 0;
    if (!n) return NULL;
    t = lxb_dom_node_text_content(as_node(n), &l);
    out = (char*)malloc(l + 1);
    if (out) {
        if (t) memcpy(out, t, l);
        out[l] = '\0';
        if (len) *len = l;
    }
    if (t) lxb_dom_document_destroy_text(as_node(n)->owner_document, t);
    return out;
}

const char* htmlq_attr(htmlq_node* n, const char* name, size_t* len) {
    size_t l = 0;
    const lxb_char_t* v;
    if (len) *len = 0;
    if (!n || as_node(n)->type != LXB_DOM_NODE_TYPE_ELEMENT) return NULL;
    v = lxb_dom_element_get_attribute(lxb_dom_interface_element(as_node(n)), (const lxb_char_t*)name, strlen(name), &l);
    if (len) *len = l;
    return (const char*)v;
}

const char* htmlq_tag(htmlq_node* n, size_t* len) {
    size_t l = 0;
    const lxb_char_t* name;
    if (len) *len = 0;
    if (!n || as_node(n)->type != LXB_DOM_NODE_TYPE_ELEMENT) return NULL;
    name = lxb_dom_element_local_name(lxb_dom_interface_element(as_node(n)), &l);
    if (len) *len = l;
    return (const char*)name;
}

htmlq_node* htmlq_parent(htmlq_node* n) {
    lxb_dom_node_t* p;
    if (!n) return NULL;
    p = as_node(n)->parent;
    return (p && p->type == LXB_DOM_NODE_TYPE_ELEMENT) ? (htmlq_node*)p : NULL;
}
