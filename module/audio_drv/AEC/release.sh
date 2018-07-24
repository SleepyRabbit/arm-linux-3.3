mkdir aaa
mv speex_echo.h aaa
mv speex_types.h aaa
mv speex_preprocess.h aaa
mv ng.h aaa
mv lpf.h aaa
mv aec_api.h aaa
rm *.c 
rm *.h
mv aaa\*.h ./
rm -rf aaa

