#include <Python.h>

#include "src/scanner.h"

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


static PyObject *py_tokenize(PyObject *self, PyObject *args) 
{
    PyObject *arg1;
    if (!PyArg_ParseTuple(args, "O:tokenize", &arg1)) {
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
            goto error_decref_unistr;
        }

        char *input = PyBytes_AsString(str);

    #endif


    if (input == NULL) {
        goto error_decref_str;
    }

    token_array *tokens = tokenize(input);
    if (tokens == NULL) {
        goto error_decref_str;
    }

    PyObject *result = PyTuple_New(tokens->n);
    if (!result) {
        token_array_destroy(tokens);
        goto error_decref_str;
        return 0;
    }

    PyObject *tuple;

    token_t token;
    for (size_t i = 0; i < tokens->n; i++) {
        token = tokens->a[i];
        tuple = Py_BuildValue("III", token.offset, token.len, token.type);
        if (PyTuple_SetItem(result, i, tuple) < 0) {
            token_array_destroy(tokens);
            goto error_decref_str;
        }
    }

    #ifndef IS_PY3K
    Py_XDECREF(str);
    #endif
    Py_XDECREF(unistr);

    token_array_destroy(tokens);

    return result;

error_decref_str:
#ifndef IS_PY3K
    Py_XDECREF(str);
#endif
error_decref_unistr:
    Py_XDECREF(unistr);
    return 0;
}

static PyMethodDef tokenize_methods[] = {
    {"tokenize", (PyCFunction)py_tokenize, METH_VARARGS, "tokenize(text)"},
    {NULL, NULL},
};



#ifdef IS_PY3K

static int tokenize_traverse(PyObject *m, visitproc visit, void *arg) {
    Py_VISIT(GETSTATE(m)->error);
    return 0;
}

static int tokenize_clear(PyObject *m) {
    Py_CLEAR(GETSTATE(m)->error);
    return 0;
}


static struct PyModuleDef module_def = {
        PyModuleDef_HEAD_INIT,
        "_tokenize",
        NULL,
        sizeof(struct module_state),
        tokenize_methods,
        NULL,
        tokenize_traverse,
        tokenize_clear,
        NULL
};

#define INITERROR return NULL

PyObject *
PyInit_tokenize(void) {
#else
#define INITERROR return

void
init_tokenize(void) {
#endif

#ifdef IS_PY3K
    PyObject *module = PyModule_Create(&module_def);
#else
    PyObject *module = Py_InitModule("_tokenize", tokenize_methods);
#endif

    if (module == NULL)
        INITERROR;
    struct module_state *st = GETSTATE(module);

    st->error = PyErr_NewException("_tokenize.Error", NULL, NULL);
    if (st->error == NULL) {
        Py_DECREF(module);
        INITERROR;
    }

#if PY_MAJOR_VERSION >= 3
    return module;
#endif
}