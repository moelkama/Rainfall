class N {
    public:
        void* vtable;        // Points to virtual methods (at offset +0)
        char annotation[100];
        int value;

        N(int val) {
            this->vtable = (void*)0x8048848;
            this->value = val;
        }

        void setAnnotation(char* src) {
            memcpy(this->annotation, src, strlen(src));
        }
};

int main(int argc, char** argv) {
    if (argc <= 1)
        _exit(1);

    N* N1 = new N(5);
    N* N2 = new N(6);

    N1->setAnnotation(argv[1]);

    (N2->vtable)(); 
}