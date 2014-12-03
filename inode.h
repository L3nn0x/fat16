/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   inode.h                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: availlan <availlan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2014/12/03 09:21:51 by availlan          #+#    #+#             */
/*   Updated: 2014/12/03 09:27:53 by availlan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */


#ifndef INODE_H
# define INODE_H

#include <string.h>

typedef struct	s_inode
{
	struct s_inode	*parent;
	struct s_inode	**leaves;
	size_t			nbEntries;
	char			filename[8];
	char			extension[3];
	size_t			size;
	char			*cache;
	unsigned char	isModified;
	unsigned char	permissions;
	uint16_t		dateModified;
	uint16_t		timeModified;
}				t_inode;

#endif /* !INODE_H */
