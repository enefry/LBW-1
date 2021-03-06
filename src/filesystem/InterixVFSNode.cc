/* © 2010 David Given.
 * LBW is licensed under the MIT open source license. See the COPYING
 * file in this distribution for the full text.
 */

#include "globals.h"
#include "filesystem/RealFD.h"
#include "filesystem/InterixVFSNode.h"
#include <sys/time.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <dirent.h>
#include <utime.h>
#include <typeinfo>

InterixVFSNode::InterixVFSNode(VFSNode* parent, const string& name, const string& path):
	VFSNode(parent, name)
{
	init(parent, name, path);
}

InterixVFSNode::InterixVFSNode(VFSNode* parent, const string& name):
	VFSNode(parent, name)
{
	init(parent, name, name);
}

void InterixVFSNode::init(VFSNode* parent, const string& name, const string& path)
{
	if (!path.empty() && (path[0] == '/'))
		_path = path;
	else
	{
		InterixVFSNode* iparent = dynamic_cast<InterixVFSNode*>(parent);
		assert(iparent);
		_path = iparent->GetRealPath() + "/" + path;
	}

	/* Ensure that the path is openable. */

	int i = chdir(_path.c_str());
	CheckError(i);
}

InterixVFSNode::~InterixVFSNode()
{
}

void InterixVFSNode::StatFile(const string& name, struct stat& st)
{
	if (name == "..")
		return GetParent()->StatFile(".", st);

	setup();
	int i = lstat(name.c_str(), &st);
	CheckError(i);
}

void InterixVFSNode::StatFS(struct statvfs& st)
{
	int result = statvfs(_path.c_str(), &st);
	CheckError(result);
}

Ref<VFSNode> InterixVFSNode::Traverse(const string& name)
{
	if ((name == ".") || (name.empty()))
		return this;
	else if (name == "..")
		return GetParent();

	return new InterixVFSNode(this, name);
}

void InterixVFSNode::setup()
{
	int i = chdir(_path.c_str());
	CheckError(i);
}

void InterixVFSNode::setup(const string& name, int e)
{
	if (name.empty())
		throw ENOENT;
	if ((name == ".") || (name == ".."))
		throw e;
	setup();
}

Ref<FD> InterixVFSNode::OpenDirectory()
{
	int newfd = open(_path.c_str(), O_RDONLY);
	CheckError(newfd);
	return new RealFD(newfd, this);
}

Ref<FD> InterixVFSNode::OpenFile(const string& name, int flags,	int mode)
{
	RAIILock locked;
	setup(name, EISDIR);

	/* Never allow opening directories --- you need to create a DirFD
	 * for this VFSNode instead.
	 */
	if (GetFileType(name) == DIRECTORY)
		throw EISDIR;

	//log("opening interix file <%s>", name.c_str());
	int newfd = open(name.c_str(), flags, mode);
	if (newfd == -1)
		throw errno;

	return new RealFD(newfd);
}

deque<string> InterixVFSNode::Enumerate()
{
	RAIILock locked;
	setup();

	deque<string> d;
	DIR* dir = opendir(".");
	for (;;)
	{
		struct dirent* de = readdir(dir);
		if (!de)
			break;
		d.push_back(de->d_name);
	}
	closedir(dir);

	return d;
}

string InterixVFSNode::ReadLink(const string& name)
{
	RAIILock locked;
	setup(name);

	char buffer[PATH_MAX];
	int i = readlink(name.c_str(), buffer, sizeof(buffer));
	if (i == -1)
		throw errno;

	return buffer;
}

void InterixVFSNode::MkDir(const string& name, int mode)
{
	RAIILock locked;

	//log("mkdir(%s %s)", GetPath().c_str(), name.c_str());

	/* Succeed silently if trying to make the current directory. */
	if (name == ".")
		return;

	setup(name);

	int i = mkdir(name.c_str(), mode);
	if (i == -1)
		throw errno;
}

void InterixVFSNode::RmDir(const string& name)
{
	RAIILock locked;
	setup(name);

	int i = rmdir(name.c_str());
	if (i == -1)
		throw errno;
}

void InterixVFSNode::Mknod(const string& name, mode_t mode, dev_t dev)
{
	RAIILock locked;
	setup(name);

	int i = mknod(name.c_str(), mode, dev);
	CheckError(i);
}

int InterixVFSNode::Access(const string& name, int mode)
{
	RAIILock locked;
	setup();

	int i = access(name.empty() ? "." : name.c_str(), mode);
	if (i == -1)
		throw errno;
	return i;
}

void InterixVFSNode::Rename(const string& from, VFSNode* other, const string& to)
{
	InterixVFSNode* othernode = dynamic_cast<InterixVFSNode*>(other);
	assert(othernode);

	if ((from == ".") || (from == "..") || from.empty() ||
		(to == ".") || (to == "..") || to.empty())
		throw EINVAL;

	RAIILock locked;

	string toabs = othernode->GetRealPath() + "/" + to;

	setup();
	int i = rename(from.c_str(), toabs.c_str());
	if (i == -1)
		throw errno;
}

void InterixVFSNode::Chmod(const string& name, int mode)
{
	RAIILock locked;
	setup();

	int i = chmod(name.c_str(), mode);
	if (i == -1)
		throw errno;
}

void InterixVFSNode::Chown(const string& name, uid_t owner, gid_t group)
{
	RAIILock locked;
	setup();

	if (Options.FakeRoot)
		return;

	int i = chown(name.c_str(), owner, group);
	if (i == -1)
		throw errno;
}

void InterixVFSNode::Link(const string& name, VFSNode* targetnode, const string& target)
{
	InterixVFSNode* itargetnode = dynamic_cast<InterixVFSNode*>(targetnode);
	assert(targetnode);

	if ((target == ".") || (target == "..") || target.empty() ||
		(name == ".") || (name == "..") || name.empty())
		throw EINVAL;

	RAIILock locked;

	string toabs = itargetnode->GetRealPath() + "/" + target;

	setup();
	int i = link(toabs.c_str(), name.c_str());
	CheckError(i);
}

void InterixVFSNode::Unlink(const string& name)
{
	RAIILock locked;
	setup(name);

	int i = unlink(name.c_str());
	if (i == -1)
	{
		/* Interix won't let us delete executables that are in use; for now
		 * just ignore these errors.
		 */
		if (errno != ETXTBUSY)
			throw errno;
	}
}

void InterixVFSNode::Symlink(const string& name, const string& target)
{
	RAIILock locked;
	setup(name);

	int i = symlink(target.c_str(), name.c_str());
	CheckError(i);
}


void InterixVFSNode::Utimes(const string& name, const struct timeval times[2])
{
	RAIILock locked;
	setup();

	/* Interix doesn't support times(), even though the docs say it does! */

	struct utimbuf ub;
	if (!times)
	{
		time(&ub.actime);
		ub.modtime = ub.actime;
	}
	else
	{
		ub.actime = times[0].tv_sec;
		ub.modtime = times[1].tv_sec;
	}

	int i = utime(name.c_str(), &ub);
	if (i == -1)
		throw errno;
}
