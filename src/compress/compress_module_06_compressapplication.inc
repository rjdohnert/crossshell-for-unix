class CompressApplication { public: int run(int argc, char* argv[]) const { return compress_main(argc, argv); } };
int main(int argc, char* argv[]) { return CompressApplication().run(argc, argv); }
