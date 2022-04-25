#!/usr/bin/awk -f
# This is a terrible hack to verify some invariants in the interpreter

function count(string, pattern,    i, n) {
    n = 0
    for (i = 1; i <= length(string); ) {
        if (match(substr(string, i), pattern)) {
            n += 1
            if (RLENGTH == 0) {
                RLENGTH = 1
            }
            i += RSTART + RLENGTH - 1
        } else {
            break
        }
    }
    return n
}

/\/\// {
    print "ERROR :" NR " Use C style comments"
}

/\/\*/ {
    comment = count($0, "/\\*")
}

/\t/ {
    print "ERROR :" NR " Use spaces not tabs"
}

/^ *# *if/ {
    print "ERROR :" NR " Don't use preprocessor conditionals."
}

comment > 1 {
    print "ERROR :" NR " More than one comment per line"
}

in_func && comment && ! / *\/\*.*\*\/ */ {
    print "ERROR " name ":" NR " Comment must be on a line of their own"
}

# Exit a special function definition
!comment && in_func && /^}/ {
    in_func = 0
    if (!has_local) {
        print "ERROR no LOCALn() in " name
    }
}

# for LOCALn()
!comment && in_func && /LOCAL/ {
    has_local = 1
}

# Demand lines start with L or a space
!comment && in_func && ! /^ *[ L] *|^$/ {
    print "ERROR " name ":" NR " line must start with [ L}]"
}

!comment && in_func && /^ +L / {
    print "ERROR " name ":" NR " extra space before L"
}

# Record if a line is allowed to have a special call
!comment && in_func {
    special = match($0, /^ *[L]/)
    assign_count = count($0, "[^!=<>]=[^=]")
    func_count = 0
}

# Demand a block statement with if, for, and while statements
!comment && in_func && /^[L ]*if / && ! /{ *$/ {
    print "ERROR " name ":" NR " if without block"
}

!comment && in_func && /^[L ]*for / && ! /{ *$/ {
    print "ERROR " name ":" NR " for without block"
}

!comment && in_func && /^[L ]*while / && ! /{ *$/ {
    print "ERROR " name ":" NR " while without block"
}

# Demand at most one assignment per line
!comment && in_func && assign_count > 1 {
    print "ERROR " name ":" NR " too many assignments " assign_count
}

# Demand at most one special call per line
!comment && in_func {
    for (pattern in funs) {
        func_count += count($0, "[^a-zA-Z0-9]" pattern "\\(")
    }
}

!comment && in_func && func_count > 1 {
    print "ERROR " name ":" NR " too many calls " func_count
}

!comment && in_func && func_count && !special {
    print "ERROR " name ":" NR " expected L on special call line"
}

# Demand RETURN macro in special functions
!comment && in_func && / return / {
    print "ERROR " name ":" NR " use return macro"
}

# Special function definition and declaration
!comment && match($0, /^DEFINE[0-9]\(([^,)]*)[^)]*\) *{/, g) {
    in_func = 1
    name = g[1]
    funs[name] = 1
    has_local = 0
}

!comment && match($0, /^DECLARE\(([^)]*)\)/, g) {
    funs[g[1]] = 1
}

/\*\// {
    comment = 0
}
