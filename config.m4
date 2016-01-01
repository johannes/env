PHP_ARG_ENABLE(env, whether to enable env support,
    [  --enable-env           Enable env support])

if test "$PHP_ENV" != "no"; then
    PHP_REQUIRE_CXX()
    PHP_NEW_EXTENSION(env, php_env.cc env.c, $ext_shared)
fi
