import csv
from sets import Set


def main():
    num_of_inodes = 0
    num_of_blocks = 0
    block_size = 0
    fragment_size = 0
    blocks_per_group = 0
    inodes_per_group = 0
    fragment_per_group = 0

    output_file = open("./lab3b_check.txt", "w")

    #----------super_csv--------------------
    super_csv = csv.reader(open("./super.csv","r"), delimiter=",")
    for row in super_csv:
        num_of_inodes = int(row[1])
        num_of_blocks = int(row[2])
        block_size = int(row[3])
        fragment_size = int(row[4])
        blocks_per_group = int(row[5])
        inodes_per_group = int(row[6])
        fragment_per_group = int(row[7])


    #-----------group_csv---------------------
    num_of_contained_blocks = []
    num_of_free_blocks = []
    num_of_free_inodes = []
    num_of_directories = []
    inode_bitmap_block = []
    block_bitmap_block = []
    inode_table_block = []

    group_csv = csv.reader(open("./group.csv","r"), delimiter=",")
    for row in group_csv:
        num_of_contained_blocks.append(int(row[0]))
        num_of_free_blocks.append(int(row[1]))
        num_of_free_inodes.append(int(row[2]))
        num_of_directories.append(int(row[3]))
        inode_bitmap_block.append(int(row[4], 16))
        block_bitmap_block.append(int(row[5], 16))
        inode_table_block.append(int(row[6], 16))


    #------------inode_csv---------------------
    allocated_inode_block_map = {}
    allocated_inode_link_count_map = {}

    inode_csv = csv.reader(open("./inode.csv","r"), delimiter=",")
    inode = 0
    for row in inode_csv:
        inode = int(row[0])
        allocated_inode_link_count_map[inode] = int(row[5])
        if inode not in allocated_inode_block_map:
            allocated_inode_block_map[inode] = []
        for i in range(15):
            allocated_inode_block_map[inode].append(int(row[11+i],16))


    #------------indirect_csv------------------
    indirect_block_map = {}
    indirect_csv = csv.reader(open("./indirect.csv","r"), delimiter=",")
    block = 0
    for row in indirect_csv:
        block = int(row[0], 16)
        if block not in indirect_block_map:
            indirect_block_map[block] = [[],[]]
        indirect_block_map[block][0].append(int(row[1])) #entry
        indirect_block_map[block][1].append(int(row[2], 16)) #block number

    #------------bitmap_csv--------------------
    free_bitmap_entry = {}
    bitmap_csv = csv.reader(open("./bitmap.csv","r"), delimiter=",")
    entry = 0

    free_block_set = Set()
    free_inode_entry = {}
    free_inode_set = Set()
    for row in bitmap_csv:
        entry = int(row[0], 16)
        if entry not in free_bitmap_entry:
            free_bitmap_entry[entry] = []
        free_bitmap_entry[entry].append(int(row[1]))
        if entry in block_bitmap_block:
            free_block_set.add(int(row[1]))
        else:
            free_inode_entry[int(row[1])] = entry
            free_inode_set.add(int(row[1]))

    #---------compute used block, unallocated block------
    for inode in range(num_of_inodes):
        if inode not in allocated_inode_block_map:
            continue
        blocks = allocated_inode_block_map[inode]
        for i in range(15):
            if blocks[i] == 0:
                break
            if blocks[i] in free_block_set:
                output_file.write("UNALLOCATED BLOCK < %d > REFERENCED BY INODE < %d > ENTRY < %d >\n" \
                %(blocks[i], inode, i))
        if blocks[12] != 0:
            # indirect blocks
            check_indirect_block(blocks[12], inode, indirect_block_map, free_block_set, output_file)
        if blocks[13] != 0:
            # second
            check_second_block(blocks[13], inode, indirect_block_map, free_block_set, output_file)
        if blocks[14] != 0:
            # triple
            check_triple_block(blocks[14], inode, indirect_block_map, free_block_set, output_file)

    #--------duplicately allocated block-------
    used_block = {}
    for inode in range(num_of_inodes):
        if inode not in allocated_inode_block_map:
            continue
        blocks = allocated_inode_block_map[inode]
        for i in range(15):
            if blocks[i] == 0:
                break
            if blocks[i] not in used_block:
                used_block[blocks[i]] = [[],[]]
            used_block[blocks[i]][0].append(inode)
            used_block[blocks[i]][1].append(i)

        if blocks[12] != 0:
            # indirect blocks
            check_duplicate_indirect_block(blocks[12], inode, indirect_block_map, used_block, output_file)
        if blocks[13] != 0:
            # second
            check_duplicate_second_block(blocks[13], inode, indirect_block_map, used_block, output_file)
        if blocks[14] != 0:
            # triple
            check_duplicate_triple_block(blocks[14], inode, indirect_block_map, used_block, output_file)

    for block in used_block.keys():
        if len(used_block[block][0]) > 1:
            inodes = used_block[block][0]
            entries = used_block[block][1]
            output_file.write("MULTIPLY REFERENCED BLOCK < %d > BY" % block)
            for i in range(len(inodes)):
                output_file.write(" INODE < %d > ENTRY < %d >" % (inodes[i], entries[i]))
            output_file.write("\n")

    #-------------unallocated inode----------------

    #-----------directory_entry--------------------
    directory_csv = csv.reader(open("directory.csv","r"), delimiter=",")
    directory = 0
    directory_entry_map = {}
    inode_parent_map = {}

    unused_inode = Set(range(11, num_of_inodes+1))
    inode_link_count = {}
    for row in directory_csv:
        directory = int(row[0])
        if directory not in directory_entry_map:
            directory_entry_map[directory] = [[],[]]
        directory_entry_map[directory][0].append(int(row[1])) #entry
        directory_entry_map[directory][1].append(int(row[4])) # inode

        inode = int(row[4])
        if row[5] != "." and row[5] != "..":
            inode_parent_map[inode] = directory

        if inode in unused_inode:
            unused_inode.remove(inode)
        if inode not in allocated_inode_link_count_map:
            output_file.write("UNALLOCATED INODE < %d > REFERENCED BY DIRECTORY < %d > ENTRY < %d >\n"\
            %(inode,directory,int(row[1])))
        # count link
        if inode not in inode_link_count:
            inode_link_count[inode] = 0
        inode_link_count[inode] = inode_link_count[inode]+1

    #------------unallocated_inode---------------
    for inode in unused_inode:
        if inode not in free_inode_entry:
            output_file.write("MISSING INODE < %d > SHOULD BE IN FREE LIST < %d >\n"\
            %(inode, inode_bitmap_block[inode/inodes_per_group]))


    #-------------incorrect link count------------
    for inode in allocated_inode_link_count_map.keys():
        if inode in inode_link_count and allocated_inode_link_count_map[inode] != inode_link_count[inode]:
            output_file.write("LINKCOUNT < %d > IS < %d > SHOULD BE < %d >\n"\
            %(inode,allocated_inode_link_count_map[inode],inode_link_count[inode]))


    #----------incorrect directory entry--------
    directory_csv = csv.reader(open("directory.csv","r"), delimiter=",")
    for row in directory_csv:
        directory = int(row[0])
        inode = int(row[4])
        if row[5] == ".":
            if inode != directory:
                output_file.write("INCORRECT ENTRY IN < %d > NAME < . > LINK TO < %d > SHOULD BE < %d >\n"\
                %(directory, inode, directory))
        if row[5] == "..":
            if directory in inode_parent_map and inode != inode_parent_map[directory]:
                output_file.write("INCORRECT ENTRY IN < %d > NAME < .. > LINK TO < %d > SHOULD BE < %d >\n"\
                %(directory, inode, inode_parent_map[directory]))


    #------------invalid block pointer-----------
    for inode in range(num_of_inodes):
        if inode not in allocated_inode_block_map:
            continue
        blocks = allocated_inode_block_map[inode]
        for i in range(15):
            if blocks[i] == 0:
                break
            if blocks[i] > num_of_blocks:
                output_file.write("INVALID BLOCK < %d > REFERENCED BY INODE < %d > ENTRY < %d >\n" \
                %(blocks[i], inode, i))
        if blocks[12] != 0:
            # indirect blocks
            check_invalid_indirect_block(blocks[12], inode, indirect_block_map, num_of_blocks, output_file)
        if blocks[13] != 0:
            # second
            check_invalid_second_block(blocks[13], inode, indirect_block_map, num_of_blocks, output_file)
        if blocks[14] != 0:
            # triple
            check_invalid_triple_block(blocks[14], inode, indirect_block_map, num_of_blocks, output_file)

    output_file.close()


def check_indirect_block(blocks, inode, indirect_block_map, free_block_set, output_file):
    if blocks in indirect_block_map:
        block = 0
        for j in range(len(indirect_block_map[blocks][0])):
            block = indirect_block_map[blocks][1][j]
            if block in free_block_set:
                output_file.write("UNALLOCATED BLOCK < %d > REFERENCED BY INODE < %d > (INDIRECT BLOCK < %d >) ENTRY < %d >)\n"\
                %(block,inode,blocks,indirect_block_map[blocks][0][j]))

def check_second_block(blocks, inode, indirect_block_map, free_block_set, output_file):
    check_indirect_block(blocks, inode, indirect_block_map, free_block_set, output_file)

    indirects = indirect_block_map[blocks][1]
    for b in indirects:
        check_indirect_block(b, inode, indirect_block_map, free_block_set, output_file)

def check_triple_block(blocks, inode, indirect_block_map, free_block_set, output_file):
    check_indirect_block(blocks, inode, indirect_block_map, free_block_set, output_file)

    indirects = indirect_block_map[blocks][1]
    for b in indirects:
        check_second_block(b, inode, indirect_block_map, free_block_set, output_file)


#-------duplicate-------------
def check_duplicate_indirect_block(blocks, inode, indirect_block_map, used_block, output_file):
    if blocks in indirect_block_map:
        block = 0
        for j in range(len(indirect_block_map[blocks][0])):
            block = indirect_block_map[blocks][1][j]

            if block not in used_block:
                used_block[block] = [[],[]]
            used_block[block][0].append(inode)
            used_block[block][1].append(indirect_block_map[blocks][0][j])

def check_duplicate_second_block(blocks, inode, indirect_block_map, used_block, output_file):
    check_duplicate_indirect_block(blocks, inode, indirect_block_map, used_block, output_file)

    indirects = indirect_block_map[blocks][1]
    for b in indirects:
        check_duplicate_indirect_block(b, inode, indirect_block_map, used_block, output_file)

def check_duplicate_triple_block(blocks, inode, indirect_block_map, used_block, output_file):
    check_duplicate_indirect_block(blocks, inode, indirect_block_map, used_block, output_file)

    indirects = indirect_block_map[blocks][1]
    for b in indirects:
        check_duplicate_second_block(b, inode, indirect_block_map, used_block, output_file)

#---------invalid----------
def check_invalid_indirect_block(blocks, inode, indirect_block_map, max_block, output_file):
    if blocks in indirect_block_map:
        block = 0
        for j in range(len(indirect_block_map[blocks][0])):
            block = indirect_block_map[blocks][1][j]
            if block > max_block:
                output_file.write("INVALID BLOCK < %d > IN INODE < %d > (INDIRECT BLOCK < %d >) ENTRY < %d >)\n"\
                %(block,inode,blocks,indirect_block_map[blocks][0][j]))

def check_invalid_second_block(blocks, inode, indirect_block_map, max_block, output_file):
    check_invalid_indirect_block(blocks, inode, indirect_block_map, max_block, output_file)

    indirects = indirect_block_map[blocks][1]
    for b in indirects:
        check_invalid_indirect_block(b, inode, indirect_block_map, max_block, output_file)

def check_invalid_triple_block(blocks, inode, indirect_block_map, max_block, output_file):
    check_invalid_indirect_block(blocks, inode, indirect_block_map, max_block, output_file)

    indirects = indirect_block_map[blocks][1]
    for b in indirects:
        check_invalid_second_block(b, inode, indirect_block_map, max_block, output_file)


main()
