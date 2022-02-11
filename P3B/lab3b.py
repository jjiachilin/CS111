# NAME: Joseph Lin
# EMAIL: jj.lin42@gmail.com
# ID: 505111868

import sys

# maps block_num to inode_num, offset, level
block_dict = {}

# maps inode_num to link count
inode_link_count_dict = {}

# maps inode_num to reference link count
inode_ref_link_count_dict = {}

# map inode_num to parent inode
inode_num_par_dict = {}

# map parent_inode_num to inode_num
inode_par_dict = {}

# maps inode_num to directory name
inode_ref_dict = {}

num_blocks = 0
num_inodes = 0
inode_size = 0
block_size = 0
first_valid = 0

bfree = set()
ifree = set()
reserved = set([0, 1, 2, 3, 4, 5, 6, 7, 64])

def parse_file(filename):
    lines = filename.readlines()
    for l in lines:
        line = l.split(",")
        if line[0] == 'SUPERBLOCK':
            global num_blocks
            global num_inodes
            global block_size
            global inode_size
            num_blocks = int(line[1])
            num_inodes = int(line[2])
            block_size = int(line[3])
            inode_size = int(line[4])
        elif line[0] == 'GROUP':
            global first_valid
            first_valid = int(line[8]) + inode_size * num_inodes / block_size
        elif line[0] == 'BFREE':
            bfree.add(int(line[1]))
        elif line[0] == 'IFREE':
            ifree.add(int(line[1]))
        elif line[0] == 'INODE':
            inode_num = int(line[1])
            inode_link_count_dict[inode_num] = int(line[6])
            for i in range(12,27):
                block_num = int(line[i])
                if block_num == 0:
                    continue

                level = ""
                offset = 0

                if i == 24:
                    level = "INDIRECT"
                    offset = 12
                elif i == 25:
                    level = "DOUBLE INDIRECT"
                    offset = 268
                elif i == 26:
                    level = "TRIPLE INDIRECT"
                    offset = 65804
                
                if block_num != 0 and block_num not in block_dict:
                    block_dict[block_num] = [[inode_num, offset, level]]
                else:
                    block_dict[block_num].append([inode_num, offset, level])
        elif line[0] == 'INDIRECT':
            inode_num = int(line[1])
            block_num = int(line[5])
            level_num = int(line[2])
            offset = int(line[3])
            level = ""

            if level_num == 1:
                level = "INDIRECT"
            elif level_num == 2:
                level = "DOUBLE INDIRECT"
            elif level_num == 3:
                level = "TRIPLE INDIRECT"

            if block_num not in block_dict:
                block_dict[block_num] = [[inode_num, offset, level]]
            else:
                block_dict[block_num].append([inode_num, offset, level])
        elif line[0] == 'DIRENT':
            dir_name = line[6][:len(line[6])-1] # remove extra new line
            par_inode = int(line[1])
            inode_num = int(line[3])

            inode_ref_dict[inode_num] = dir_name

            if inode_num not in inode_ref_link_count_dict:
                inode_ref_link_count_dict[inode_num] = 1
            else:
                inode_ref_link_count_dict[inode_num] += 1

            if dir_name == "'..'":
                inode_par_dict[par_inode] = inode_num
            else:
                inode_num_par_dict[inode_num] = par_inode

def check_data_block_num():
    r = True

    for block_num in block_dict:
        cur_block = block_dict[block_num][0]

        # invalid
        if block_num < 0 or block_num > num_blocks:
            if len(cur_block[2]) > 0:
                print('INVALID ' + cur_block[2] + ' BLOCK ' + str(block_num) + ' IN INODE ' + str(cur_block[0]) + ' AT OFFSET ' + str(cur_block[1]))
            else:
                print('INVALID BLOCK ' + str(block_num) + ' IN INODE ' + str(cur_block[0]) + ' AT OFFSET ' + str(cur_block[1]))
            r = False

        # reserved
        global first_valid
        if block_num > 0 and block_num < first_valid:
            if len(cur_block[2]) > 0:
                print('RESERVED ' + cur_block[2] + ' BLOCK ' + str(block_num) + ' IN INODE ' + str(cur_block[0]) + ' AT OFFSET ' + str(cur_block[1]))
            else:
                print('RESERVED BLOCK ' + str(block_num) + ' IN INODE ' + str(cur_block[0]) + ' AT OFFSET ' + str(cur_block[1]))
            r = False

        # duplicates
        if len(block_dict[block_num]) > 1:
            for b in block_dict[block_num]:
                if len(b[2]) > 0:
                    print('DUPLICATE ' + b[2] + ' BLOCK ' + str(block_num) + ' IN INODE ' + str(b[0]) + ' AT OFFSET ' + str(b[1]))
                else:
                    print('DUPLICATE BLOCK ' + str(block_num) + ' IN INODE ' + str(b[0]) + ' AT OFFSET ' + str(b[1]))
                r = False

    for b in range(1, num_blocks + 1):
        # unreferenced
        if b not in bfree and b not in block_dict and b not in reserved:
            print('UNREFERENCED BLOCK ' + str(b))
            r = False

        # allocated but marked free
        elif b in bfree and b in block_dict:
            print('ALLOCATED BLOCK ' + str(b) + ' ON FREELIST')
            r = False
    
    return r

def check_inode():
    r = True

    for i in range(1, num_inodes + 1):
        # allocated
        if i in inode_link_count_dict and i in ifree:
            print('ALLOCATED INODE ' + str(i) + ' ON FREELIST')
            r = False

        # unallocated    
        elif i not in ifree and i not in inode_link_count_dict and i not in inode_num_par_dict and i not in (1, 3, 4, 5, 6, 7, 8, 9, 10):
            print('UNALLOCATED INODE ' + str(i) + ' NOT ON FREELIST')
            r = False

    return r

def check_dir():
    r = True
    global num_inodes
    for inode_num in inode_link_count_dict:
        # invalid part 1
        if inode_num < 1 or inode_num > num_inodes:
            print('DIRECTORY INODE ' + str(inode_num_par_dict[inode_num]) + ' NAME ' + inode_ref_dict[inode_num] + ' INVALID INODE ' + str(inode_num))
            r = False

        # . not pointing to cur dir
        if inode_num in inode_ref_dict and inode_ref_dict[inode_num] == "'.'" and inode_num_par_dict[inode_num] != inode_num:
            print('DIRECTORY INODE ' + str(inode_num_par_dict[inode_num]) + " NAME '.' LINK TO INODE " + str(inode_num) + ' SHOULD BE ' + str(inode_num_par_dict[inode_num]))
            r = False

        # wrong link count
        links = 0
        if inode_num in inode_ref_link_count_dict:
            links = inode_ref_link_count_dict[inode_num]

        if links != inode_link_count_dict[inode_num]:
            print('INODE ' + str(inode_num) + ' HAS ' + str(links) + ' LINKS BUT LINKCOUNT IS ' + str(inode_link_count_dict[inode_num]))
            r = False
    
    # invalid part 2
    for inode_num in inode_ref_dict:
        if inode_num < 1 or inode_num > num_inodes:
            print('DIRECTORY INODE ' + str(inode_num_par_dict[inode_num]) + ' NAME ' + inode_ref_dict[inode_num] + ' INVALID INODE ' + str(inode_num))
            r = False

    # unallocated
    for inode_num in inode_ref_link_count_dict:
        if inode_num in ifree and inode_num in inode_num_par_dict:
            print('DIRECTORY INODE ' + str(inode_num_par_dict[inode_num]) + ' NAME ' + inode_ref_dict[inode_num] + ' UNALLOCATED INODE ' + str(inode_num))
            r = False

    # .. not pointing to par dir
    for parent_num in inode_par_dict:
        if parent_num == 2 and inode_par_dict[parent_num] == 2:
            continue

        elif parent_num == 2:
            print("DIRECTORY INODE 2 NAME '..' LINK TO INODE " + str(inode_par_dict[parent_num]) + ' SHOULD BE 2')
            r = False
        elif parent_num not in inode_num_par_dict:
            print('DIRECTORY INODE ' + str(inode_par_dict[parent_num]) + " NAME '..' LINK TO INODE " + str(parent_num) + ' SHOULD BE ' + str(inode_par_dict[parent_num]))
            r = False
        elif parent_num != inode_num_par_dict[parent_num]:
            print('DIRECTORY INODE ' + str(parent_num) + " NAME '..' LINK TO INODE " + str(parent_num) + ' SHOULD BE ' + str(inode_num_par_dict[parent_num]))
            r = False
    
    return r

def main():
    try:
        filename = open(sys.argv[1], "r")
    except:
        sys.stderr.write('Error, file does not exist\n')
        exit(1)

    parse_file(filename)
   
    if not check_data_block_num() or not check_inode() or not check_dir():
        exit(2)
    else:
        exit(0)

if __name__ == '__main__':
    main()