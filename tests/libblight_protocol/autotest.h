#pragma once
#include <QTest>
#include <QList>
#include <QString>
#include <QSharedPointer>

/**
 * @brief Retrieves the static list of registered test objects.
 *
 * This function returns a reference to a unique, static list that holds pointers to QObject instances
 * representing registered tests. The list persists for the lifetime of the application.
 *
 * @return TestList& A reference to the static test list.
 */

/**
 * @brief Determines if a test object is already registered.
 *
 * This function checks whether the given QObject pointer is present in the static test list by comparing
 * both the pointer and the object's name.
 *
 * @param object Pointer to the QObject to search for.
 * @return bool True if the object or an object with a matching name is found, false otherwise.
 */

/**
 * @brief Registers a new test object if it is not already registered.
 *
 * This function adds the provided QObject to the static test list only if no object with the same address
 * or object name is already present.
 *
 * @param object Pointer to the QObject representing the test to add.
 */

/**
 * @brief Executes all registered tests.
 *
 * This function runs each test in the static test list using QTest::qExec, passing the command-line arguments,
 * and returns the total number of test failures encountered.
 *
 * @param argc The number of command-line arguments.
 * @param argv An array of command-line argument strings.
 * @return int The total count of test failures.
 */
namespace AutoTest{
typedef QList<QObject*> TestList;
inline TestList& testList(){
    static TestList list;
    return list;
}
inline bool findObject(QObject* object){
    TestList& list = testList();
    if(list.contains(object)){
        return true;
    }
    foreach(QObject* test, list){
        if(test->objectName() == object->objectName()){
            return true;
        }
    }
    return false;
}
inline void addTest(QObject* object){
    TestList& list = testList();
    if(!findObject(object)){
        list.append(object);
    }
}
inline int run(int argc, char *argv[]){
    int ret = 0;
    foreach(QObject* test, testList()){
        ret += QTest::qExec(test, argc, argv);
    }
    return ret;
}
}

template <class T>
class Test{
public:
    QSharedPointer<T> child;

    /**
     * @brief Constructs a test instance and registers it with the AutoTest framework.
     *
     * Initializes a new test object of type T by wrapping it in a QSharedPointer,
     * setting its object name to the provided value, and adding it to the global test list.
     *
     * @param name The unique name used to identify the test instance.
     */
    Test(const QString& name) : child(new T){
        child->setObjectName(name);
        AutoTest::addTest(child.data());
    }
};

#define DECLARE_TEST(className) static Test<className> t(#className);
