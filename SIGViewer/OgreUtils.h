#ifndef _SIG_OGRE_UTILS_H
#define _SIG_OGRE_UTILS_H

// A small collection of utility functions for dealing with Ogre stuff

void logAllNodeChildren(Ogre::Node* node, unsigned int depth)
{
    std::string indent(depth, '-');
    Ogre::LogManager::getSingleton().logMessage(Ogre::LML_NORMAL, indent + " " + node->getName());
    auto childit = node->getChildIterator();
    while (childit.hasMoreElements())
    {
        logAllNodeChildren(childit.getNext(), depth + 1);
    }
}

Ogre::Node* findChildRecursive(Ogre::Node* parent, std::string name)
{
    if (parent == NULL || parent->getName() == name)
        return parent;

    Ogre::Node* result = NULL;
    auto childit = parent->getChildIterator();
    while (childit.hasMoreElements())
    {
        auto temp = findChildRecursive(childit.getNext(), name);
        if (temp != NULL)
            result = temp;
    }
    return result;
}

#endif