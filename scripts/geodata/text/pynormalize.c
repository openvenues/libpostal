#include <Python.h>

#include "src/normalize.h"
#include "src/transliterate.h"

#if PY_MAJOR_VERSION >= 3
#define IS_PY3K
#endif

struct module_state {
    PyObject *error;
};


#ifdef IS_PY3K
    #define GETSTATE(m) ((struct module_state*)PyModule_GetState(m))
#else
    #define GETSTATE(m) (&_state)
    static struct module_state _state;
#endif



static PyObject *py_normalize_string_utf8(PyObject *self, PyObject *args) 
{
    PyObject *arg1;
    uint64_t options;
    if (!PyArg_ParseTuple(args, "OK:normalize", &arg1, &options)) {
        return 0;
    }

    PyObject *unistr = PyUnicode_FromObject(arg1);
    if (unistr == NULL) {
        PyErr_SetString(PyExc_TypeError,
                        "Parameter could not be converted to unicode in scanner");
        return 0;
    }

    #ifdef IS_PY3K
        // Python 3 encoding, supported by Python 3.3+

        char *input = PyUnicode_AsUTF8(unistr);

    #else
        // Python 2 encoding

        PyObject *str = PyUnicode_AsEncodedString(unistr, "utf-8", "strict");
        if (str == NULL) {
            PyErr_SetString(PyExc_TypeError,
                            "Parameter could not be utf-8 encoded");
            goto exit_decref_unistr;
        }

        char *input = PyBytes_AsString(str);

    #endif

    if (input == NULL) {
        goto exit_decref_str;
    }

    char *normalized = normalize_string_utf8(input, options);

    if (normalized == NULL) {
        goto exit_decref_str;
    }

    PyObject *result = PyUnicode_DecodeUTF8((const char *)normalized, strlen(normalized), "strict");
    free(normalized);
    if (result == NULL) {
            PyErr_SetString(PyExc_ValueError,
                            "Result could not be utf-8 decoded");
            goto exit_decref_str;
    }

    #ifndef IS_PY3K
    Py_XDECREF(str);
    #endif
    Py_XDECREF(unistr);

    return result;

exit_decref_str:
#ifndef IS_PY3K
    Py_XDECREF(str);
#endif
exit_decref_unistr:
    Py_XDECREF(unistr);
    return 0;
}


static PyObject *py_normalize_string_latin(PyObject *self, PyObject *args) 
{
    PyObject *arg1;
    uint64_t options;
    if (!PyArg_ParseTuple(args, "OK:normalize", &arg1, &options)) {
        return 0;
    }

    PyObject *unistr = PyUnicode_FromObject(arg1);
    if (unistr == NULL) {
        PyErr_SetString(PyExc_TypeError,
                        "Parameter could not be converted to unicode in scanner");
        return 0;
    }

    #ifdef IS_PY3K
        // Python 3 encoding, supported by Python 3.3+

        char *input = PyUnicode_AsUTF8(unistr);

    #else
        // Python 2 encoding

        PyObject *str = PyUnicode_AsEncodedString(unistr, "utf-8", "strict");
        if (str == NULL) {
            PyErr_SetString(PyExc_TypeError,
                            "Parameter could not be utf-8 encoded");
            goto exit_decref_unistr;
        }

        char *input = PyBytes_AsString(str);

    #endif

    if (input == NULL) {
        goto exit_decref_str;
    }

    char *normalized = normalize_string_latin(input, strlen(input), options);

    PyObject *result = PyUnicode_DecodeUTF8((const char *)normalized, strlen(normalized), "strict");
    free(normalized);
    if (result == NULL) {
        PyErr_SetString(PyExc_ValueError,
                        "Result could not be utf-8 decoded");
        goto exit_decref_str;
    }

    #ifndef IS_PY3K
    Py_XDECREF(str);
    #endif
    Py_XDECREF(unistr);

    return result;

exit_decref_str:
#ifndef IS_PY3K
    Py_XDECREF(str);
#endif
exit_decref_unistr:
    Py_XDECREF(unistr);
    return 0;
}



static PyObject *py_normalize_token(PyObject *self, PyObject *args) 
{
    PyObject *s;

    uint32_t offset;
    uint32_t len;
    uint16_t type;

    uint64_t options;
    if (!PyArg_ParseTuple(args, "O(IIH)K:normalize", &s, &offset, &len, &type, &options)) {
        PyErr_SetString(PyExc_TypeError,
                        "Error parsing arguments");
        return 0;
    }

    token_t token = (token_t){(size_t)offset, (size_t)len, type};

    PyObject *unistr = PyUnicode_FromObject(s);
    if (unistr == NULL) {
        PyErr_SetString(PyExc_TypeError,
                        "Parameter could not be converted to unicode in scanner");
        return 0;
    }

    #ifdef IS_PY3K
        // Python 3 encoding, supported by Python 3.3+

        char *input = PyUnicode_AsUTF8(unistr);

    #else
        // Python 2 encoding

        PyObject *str = PyUnicode_AsEncodedString(unistr, "utf-8", "strict");
        if (str == NULL) {
            PyErr_SetString(PyExc_ValueError,
                            "Parameter could not be utf-8 encoded");
            goto exit_decref_unistr;
        }

        char *input = PyBytes_AsString(str);

    #endif

    if (input == NULL) {
        goto exit_decref_str;
    }

    char_array *token_buffer = char_array_new_size(token.len);

    add_normalized_token(token_buffer, input, token, options);
    char *token_str = char_array_get_string(token_buffer);
    PyObject *result = PyUnicode_DecodeUTF8((const char *)token_str, token_buffer->n - 1, "strict");

    if (result == NULL) {
        PyErr_SetString(PyExc_ValueError,
                        "Error decoding token");
        char_array_destroy(token_buffer);
        goto exit_decref_str;
    }

    char_array_destroy(token_buffer);

    #ifndef IS_PY3K
    Py_XDECREF(str);
    #endif
    Py_XDECREF(unistr);

    return result;

exit_decref_str:
#ifndef IS_PY3K
    Py_XDECREF(str);
#endif
exit_decref_unistr:
    Py_XDECREF(unistr);
    return 0;
}

static PyMethodDef normalize_methods[] = {
    {"normalize_string_utf8", (PyCFunction)py_normalize_string_utf8, METH_VARARGS, "normalize_string_utf8(input, options)"},
    {"normalize_string_latin", (PyCFunction)py_normalize_string_latin, METH_VARARGS, "normalize_string_latin(input, options)"},
    {"normalize_token", (PyCFunction)py_normalize_token, METH_VARARGS, "normalize_token(input, options)"},
    {NULL, NULL},
};



#ifdef IS_PY3K

static int normalize_traverse(PyObject *m, visitproc visit, void *arg) {
    Py_VISIT(GETSTATE(m)->error);
    return 0;
}

static int normalize_clear(PyObject *m) {
    Py_CLEAR(GETSTATE(m)->error);
    return 0;
}


static struct PyModuleDef module_def = {
        PyModuleDef_HEAD_INIT,
        "_normalize",
        NULL,
        sizeof(struct module_state),
        normalize_methods,
        NULL,
        normalize_traverse,
        normalize_clear,
        NULL
};

#define INITERROR return NULL

PyObject *
PyInit_normalize(void) {
#else
#define INITERROR return

void
init_normalize(void) {
#endif

#ifdef IS_PY3K
    PyObject *module = PyModule_Create(&module_def);
#else
    PyObject *module = Py_InitModule("_normalize", normalize_methods);
#endif

    if (module == NULL)
        INITERROR;
    struct module_state *st = GETSTATE(module);

    st->error = PyErr_NewException("_normalize.Error", NULL, NULL);
    if (st->error == NULL) {
        Py_DECREF(module);
        INITERROR;
    }

    if (!transliteration_module_setup(NULL)) {
        PyErr_SetString(PyExc_RuntimeError,
                        "Could not load transliterate module");
        Py_DECREF(module);
        INITERROR;
    }

    PyModule_AddObject(module, "NORMALIZE_STRING_LATIN_ASCII", PyLong_FromUnsignedLongLong(NORMALIZE_STRING_LATIN_ASCII));
    PyModule_AddObject(module, "NORMALIZE_STRING_TRANSLITERATE", PyLong_FromUnsignedLongLong(NORMALIZE_STRING_TRANSLITERATE));
    PyModule_AddObject(module, "NORMALIZE_STRING_STRIP_ACCENTS", PyLong_FromUnsignedLongLong(NORMALIZE_STRING_STRIP_ACCENTS));
    PyModule_AddObject(module, "NORMALIZE_STRING_DECOMPOSE", PyLong_FromUnsignedLongLong(NORMALIZE_STRING_DECOMPOSE));
    PyModule_AddObject(module, "NORMALIZE_STRING_COMPOSE", PyLong_FromUnsignedLongLong(NORMALIZE_STRING_COMPOSE));
    PyModule_AddObject(module, "NORMALIZE_STRING_LOWERCASE", PyLong_FromUnsignedLongLong(NORMALIZE_STRING_LOWERCASE));
    PyModule_AddObject(module, "NORMALIZE_STRING_TRIM", PyLong_FromUnsignedLongLong(NORMALIZE_STRING_TRIM));
    PyModule_AddObject(module, "NORMALIZE_STRING_REPLACE_HYPHENS", PyLong_FromUnsignedLongLong(NORMALIZE_STRING_REPLACE_HYPHENS));
    PyModule_AddObject(module, "NORMALIZE_STRING_SIMPLE_LATIN_ASCII", PyLong_FromUnsignedLongLong(NORMALIZE_STRING_SIMPLE_LATIN_ASCII));


    PyModule_AddObject(module, "NORMALIZE_TOKEN_REPLACE_HYPHENS", PyLong_FromUnsignedLongLong(NORMALIZE_TOKEN_REPLACE_HYPHENS));
    PyModule_AddObject(module, "NORMALIZE_TOKEN_DELETE_HYPHENS", PyLong_FromUnsignedLongLong(NORMALIZE_TOKEN_DELETE_HYPHENS));
    PyModule_AddObject(module, "NORMALIZE_TOKEN_DELETE_FINAL_PERIOD", PyLong_FromUnsignedLongLong(NORMALIZE_TOKEN_DELETE_FINAL_PERIOD));
    PyModule_AddObject(module, "NORMALIZE_TOKEN_DELETE_ACRONYM_PERIODS", PyLong_FromUnsignedLongLong(NORMALIZE_TOKEN_DELETE_ACRONYM_PERIODS));
    PyModule_AddObject(module, "NORMALIZE_TOKEN_DROP_ENGLISH_POSSESSIVES", PyLong_FromUnsignedLongLong(NORMALIZE_TOKEN_DROP_ENGLISH_POSSESSIVES));
    PyModule_AddObject(module, "NORMALIZE_TOKEN_DELETE_OTHER_APOSTROPHE", PyLong_FromUnsignedLongLong(NORMALIZE_TOKEN_DELETE_OTHER_APOSTROPHE));
    PyModule_AddObject(module, "NORMALIZE_TOKEN_SPLIT_ALPHA_FROM_NUMERIC", PyLong_FromUnsignedLongLong(NORMALIZE_TOKEN_SPLIT_ALPHA_FROM_NUMERIC));
    PyModule_AddObject(module, "NORMALIZE_TOKEN_REPLACE_DIGITS", PyLong_FromUnsignedLongLong(NORMALIZE_TOKEN_REPLACE_DIGITS));


#if PY_MAJOR_VERSION >= 3
    return module;
#endif
}
