#ifndef SCAN_SCAN_H
#define SCAN_SCAN_H
#include <vector>
#include <iostream>

class T;

class NotImplemented;

class Scan {
    public:
        explicit Scan(const std::vector<long long>& array) : array(array) {}
        void getScanned(std::vector<long long>& result) const;
        void getSScannedConst(std::vector<long long>& result) const;
        void getSScanned(std::vector<long long>& result);

        void acceptTConst(T t) const;
        void acceptT(T t);
        void acceptTConstConst(const T t) const;
        void acceptTConst(const T t);
        void acceptTRefConst(T& t) const;
        void acceptTRef(T& t);
        void acceptTRvalConst(T&& t) const;
        void acceptTRval(T&& t);
        void acceptTConstRefConst(const T& t) const;
        void acceptTConstRef(const T& t);
        void acceptTPointerConst(T* t) const;
        void acceptTPointer(T* t);
        void acceptTConstPointerConst(const T* t) const;
        void acceptTConstPointer(const T* t);

        void acceptNotImplementedConst(NotImplemented t) const;
        void acceptNotImplemented(NotImplemented t);
        void acceptNotImplementedConstConst(const NotImplemented t) const;
        void acceptNotImplementedConst(const NotImplemented t);
        void acceptNotImplementedRefConst(NotImplemented& t) const;
        void acceptNotImplementedRef(NotImplemented& t);
        void acceptNotImplementedRvalConst(NotImplemented&& t) const;
        void acceptNotImplementedRval(NotImplemented&& t);
        void acceptNotImplementedConstRefConst(const NotImplemented& t) const;
        void acceptNotImplementedConstRef(const NotImplemented& t);
        void acceptNotImplementedPointerConst(NotImplemented* t) const;
        void acceptNotImplementedPointer(NotImplemented* t);
        void acceptNotImplementedConstPointerConst(const NotImplemented* t) const;
        void acceptNotImplementedConstPointer(const NotImplemented* t);
        // void commented func(Comment bla t) const;
        //  void commented func(Comment bla t) const;
        template<typename T, typename R>
        class Generic {
            public:
                explicit Generic(T t, R r) : t(t), r(r) {}
                template<typename K>
                class InternalGeneric;
                T t;
                R r;
        };

        void acceptGenericConst(Generic<int, long> t) const;
        void acceptGeneric(Generic<int, long> t);
        void acceptGenericConstConst(const Generic<int, long> t) const;
        void acceptGenericByConst(const Generic<int, long> t);
        void acceptGenericRefConst(Generic<int, long>& t) const;
        void acceptGenericRef(Generic<int, long>& t);
        void acceptGenericRvalConst(Generic<int, long>&& t) const;
        void acceptGenericRval(Generic<int, long>&& t);
        void acceptGenericConstRefConst(const Generic<int, long>& t) const;
        void acceptGenericConstRef(const Generic<int, long>& t);
        void acceptGenericPointerConst(Generic<int, long>* t) const;
        void acceptGenericPointer(Generic<int, long>* t);
        void acceptGenericConstPointerConst(const Generic<int, long>* t) const;

        template<typename T, typename R>
        class GenericNotImplemented;

        void acceptGenericNotImplementedConst(GenericNotImplemented<int, long> t) const;
        void acceptGenericNotImplemented(GenericNotImplemented<int, long> t);
        void acceptGenericNotImplementedConstConst(const GenericNotImplemented<int, long> t) const;
        void acceptGenericNotImplementedByConst(const GenericNotImplemented<int, long> t);
        void acceptGenericNotImplementedRefConst(GenericNotImplemented<int, long>& t) const;
        void acceptGenericNotImplementedRef(GenericNotImplemented<int, long>& t);
        void acceptGenericNotImplementedRvalConst(GenericNotImplemented<int, long>&& t) const;
        void acceptGenericNotImplementedRval(GenericNotImplemented<int, long>&& t);
        void acceptGenericNotImplementedConstRefConst(const GenericNotImplemented<int, long>& t) const;
        void acceptGenericNotImplementedConstRef(const GenericNotImplemented<int, long>& t);
        void acceptGenericNotImplementedPointerConst(GenericNotImplemented<int, long>* t) const;
        void acceptGenericNotImplementedPointer(GenericNotImplemented<int, long>* t);
        void acceptGenericNotImplementedConstPointerConst(const GenericNotImplemented<int, long>* t) const;
        void acceptGenericNotImplementedConstPointer(const GenericNotImplemented<int, long>* t);

        class InternalScan {
            public:
                explicit InternalScan(const Scan& scan) : scan(scan) {}
                void getScanned(std::vector<long long>& result) const;
                //  void commented func(Comment bla t) const;

                class InternalInternal {
                    class InternalInternalInternalNotImplemented;
                    void acceptInternalNotImplemented(InternalInternalInternalNotImplemented t);

                };

                class InternalInternalNotImplemented;

                void acceptInternalNotImplementedConst(InternalInternalNotImplemented t) const;
                void acceptInternalNotImplemented(InternalInternalNotImplemented t);
                void acceptInternalNotImplementedConstConst(const InternalInternalNotImplemented t) const;
                void acceptInternalNotImplementedConst(const InternalInternalNotImplemented t);
                void acceptInternalNotImplementedRefConst(InternalInternalNotImplemented& t) const;
                void acceptInternalNotImplementedRef(InternalInternalNotImplemented& t);
                void acceptInternalNotImplementedRvalConst(InternalInternalNotImplemented&& t) const;
                void acceptInternalNotImplementedRval(InternalInternalNotImplemented&& t);
                void acceptInternalNotImplementedConstRefConst(const InternalInternalNotImplemented& t) const;
                void acceptInternalNotImplementedConstRef(const InternalInternalNotImplemented& t);
                void acceptInternalNotImplementedPointerConst(InternalInternalNotImplemented* t) const;
                void acceptInternalNotImplementedPointer(InternalInternalNotImplemented* t);
                void acceptInternalNotImplementedConstPointerConst(const InternalInternalNotImplemented* t) const;
                void acceptInternalNotImplementedConstPointer(const InternalInternalNotImplemented* t);

            private:
                //  void commented func(Comment bla t) const;
                const Scan& scan;
        };

        class InternalNotImplemented;

        void acceptInternalNotImplementedConst(InternalNotImplemented t) const;
        void acceptInternalNotImplemented(InternalNotImplemented t);
        void acceptInternalNotImplementedConstConst(const InternalNotImplemented t) const;
        void acceptInternalNotImplementedConst(const InternalNotImplemented t);
        void acceptInternalNotImplementedRefConst(InternalNotImplemented& t) const;
        void acceptInternalNotImplementedRef(InternalNotImplemented& t);
        void acceptInternalNotImplementedRvalConst(InternalNotImplemented&& t) const;
        void acceptInternalNotImplementedRval(InternalNotImplemented&& t);
        void acceptInternalNotImplementedConstRefConst(const InternalNotImplemented& t) const;
        void acceptInternalNotImplementedConstRef(const InternalNotImplemented& t);
        void acceptInternalNotImplementedPointerConst(InternalNotImplemented* t) const;
        void acceptInternalNotImplementedPointer(InternalNotImplemented* t);
        void acceptInternalNotImplementedConstPointerConst(const InternalNotImplemented* t) const;
        void acceptInternalNotImplementedConstPointer(const InternalNotImplemented* t);

        InternalScan getInternal() const;

    private:
        const std::vector<long long>& array;

};

class Scan::InternalScan::InternalInternalNotImplemented {};

class KeywordTestBase {
    public:
        virtual ~KeywordTestBase() = default;
        virtual void pureMethod() = 0;
        virtual void virtualDecl();
        virtual void virtualDefined() {}
};

class KeywordTestClass : public KeywordTestBase {
    public:
        KeywordTestClass();
        ~KeywordTestClass();
        explicit KeywordTestClass(int dummy);
        KeywordTestClass(const KeywordTestClass&) = default;
        KeywordTestClass(KeywordTestClass&&) = delete;

        void pureMethod() override;

        static int staticDecl();
        static int staticDefined() { return 0; }

        inline void inlineDecl();
        inline void inlineDefined() {}

        constexpr int constexprDefined() const { return 42; }

        void noexceptDecl() noexcept;
        void constNoexceptDecl() const noexcept;
        int noexceptReturning() noexcept;

        friend void keywordFriendFunc(KeywordTestClass& k);
        friend class KeywordFriendClass;

        using BaseType = KeywordTestBase;

        typedef int IntAlias;
};

extern "C" void externCFunc();

constexpr int constexprFreeFunc() { return 1; }

using ScanAlias = Scan;

typedef int ScanInt;

static_assert(sizeof(int) >= 4, "int must be at least 4 bytes");


namespace stubtest {
    void freeInNamespace();
    inline void freeInNamespaceInline() {}
    constexpr int constexprInNamespace() { return 3; }
    static int staticFreeInNamespace();
    class NotImplemented;


    namespace inner {
        class NotImplemented;

        void deepFreeFunc();
    }
}


class T {
    public:
        int get() const;
};

void Scan::InternalScan::getScanned(std::vector<long long>& result) const {
    scan.getScanned(result);
}
#endif //SCAN_SCAN_H