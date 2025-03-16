#pragma once
#include <QTest>
#include <QList>
#include <QString>
#include <QSharedPointer>

/**
 * @brief Returns the singleton test list.
 *
 * Provides access to a static list of test objects used by the AutoTest framework.
 *
 * @return TestList& A reference to the list holding pointers to registered test objects.
 */
 
/**
 * @brief Determines whether a test object is already registered.
 *
 * Checks if the provided QObject exists in the test list, either by direct pointer comparison or by matching object names.
 *
 * @param object Pointer to the QObject to search for.
 * @return bool True if the object is found in the list; otherwise, false.
 */
 
/**
 * @brief Adds a test object to the list if it is not already present.
 *
 * Registers the provided QObject with the AutoTest framework by appending it to the static test list if no existing object with the same pointer or name is found.
 *
 * @param object Pointer to the QObject representing the test to be added.
 */
 
/**
 * @brief Executes all registered tests.
 *
 * Iterates over all test objects in the test list, executing each test via QTest::qExec, and aggregates their return codes.
 *
 * @param argc Number of command line arguments.
 * @param argv Array of command line argument strings.
 * @return int The cumulative result of all test executions.
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
     * This constructor creates a new instance of type T, sets its object name to the provided
     * name, and adds the instance to the global test registry through AutoTest::addTest.
     *
     * @param name The name to assign to the test object.
     */
    Test(const QString& name) : child(new T){
        child->setObjectName(name);
        AutoTest::addTest(child.data());
    }
};

#define DECLARE_TEST(className) static Test<className> t(#className);
