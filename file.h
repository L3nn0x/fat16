/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   file.h                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: availlan <availlan@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2014/12/03 09:21:51 by availlan          #+#    #+#             */
/*   Updated: 2014/12/03 10:03:45 by availlan         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */


#ifndef INODE_H
# define INODE_H

#include <string.h>

typedef struct	s_file
{
	struct s_file	*parent;
	struct s_file	*leaf;
	char			filename[8];
	char			extension[3];
	size_t			size;
	char			*cache;
	unsigned char	isModified;
	unsigned char	permissions;
	uint16_t		dateModified;
	uint16_t		timeModified;
	struct s_file	*next;
	struct s_file	*prev;
}				t_file;

#endif /* !INODE_H */
